// Copyright (C) 2011 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

// @fileoverview Poor-man's unittests for some of the trickier logic bits of
// the flakiness dashboard.
//
// Currently only tests processExpectations and populateExpectationsData.
// A test just consists of calling runExpectationsTest with the appropriate
// arguments.

// FIXME: move this over to using qunit

// Clears out the global objects modified or used by processExpectations and
// populateExpectationsData. A bit gross since it's digging into implementation
// details, but it's good enough for now.
function setupExpectationsTest()
{
    allExpectations = null;
    allTests = null;
    g_expectationsByTest = {};
    g_resultsByBuilder = {};
    g_builders = {};
    g_allExpectations = null;
    g_allTests = null;
}

// Processes the expectations for a test and asserts that the final expectations
// and modifiers we apply to a test match what we expect.
//
// @param {string} builder Builder the test is run on.
// @param {string} test The test name.
// @param {string} expectations Sorted string of what the expectations for this
//        test ought to be for this builder.
// @param {string} modifiers Sorted string of what the modifiers for this
//        test ought to be for this builder.
function runExpectationsTest(builder, test, expectations, modifiers)
{
    g_builders[builder] = true;

    // Put in some dummy results. processExpectations expects the test to be
    // there.
    var tests = {};
    tests[test] = {'results': [[100, 'F']], 'times': [[100, 0]]};
    g_resultsByBuilder[builder] = {'tests': tests};

    processExpectations();
    var resultsForTest = createResultsObjectForTest(test, builder);
    populateExpectationsData(resultsForTest);

    var message = 'Builder: ' + resultsForTest.builder + ' test: ' + resultsForTest.test;
    assertEquals(resultsForTest.expectations, expectations, message);
    assertEquals(resultsForTest.modifiers, modifiers, message);
}

function assertEquals(actual, expected, message)
{
    if (expected !== actual) {
        if (message)
            message += ' ';
        else
            message = '';

        throw Error(message + 'got: ' + actual + ' expected: ' + expected);
    }
}

function throwError(resultsForTests, actual, expected) {}

function testReleaseFail()
{
    var builder = 'Webkit Win';
    var test = 'foo/1.html';
    var expectationsArray = [
        {'modifiers': 'RELEASE', 'expectations': 'FAIL'}
    ];
    g_expectationsByTest[test] = expectationsArray;
    runExpectationsTest(builder, test, 'FAIL', 'RELEASE');
}

function testReleaseFailDebugCrashReleaseBuilder()
{
    var builder = 'Webkit Win';
    var test = 'foo/1.html';
    var expectationsArray = [
        {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
        {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
    ];
    g_expectationsByTest[test] = expectationsArray;
    runExpectationsTest(builder, test, 'FAIL', 'RELEASE');
}

function testReleaseFailDebugCrashDebugBuilder()
{
    var builder = 'Webkit Win (dbg)';
    var test = 'foo/1.html';
    var expectationsArray = [
        {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
        {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
    ];
    g_expectationsByTest[test] = expectationsArray;
    runExpectationsTest(builder, test, 'CRASH', 'DEBUG');
}

function testOverrideJustBuildType()
{
    var test = 'bar/1.html';
    g_expectationsByTest['bar'] = [
        {'modifiers': 'WONTFIX', 'expectations': 'FAIL PASS TIMEOUT'}
    ];
    g_expectationsByTest[test] = [
        {'modifiers': 'WONTFIX MAC', 'expectations': 'FAIL'},
        {'modifiers': 'LINUX DEBUG', 'expectations': 'CRASH'},
    ];
    runExpectationsTest('Webkit Win', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
    runExpectationsTest('Webkit Win (dbg)(3)', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
    runExpectationsTest('Webkit Linux', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
    runExpectationsTest('Webkit Linux (dbg)(3)', test, 'CRASH', 'LINUX DEBUG');
    runExpectationsTest('Webkit Mac10.5', test, 'FAIL', 'WONTFIX MAC');
    runExpectationsTest('Webkit Mac10.5 (dbg)(3)', test, 'FAIL', 'WONTFIX MAC');
}

function testPlatformAndBuildType()
{
    var runPlatformAndBuildTypeTest = function(builder, expectedPlatform, expectedBuildType) {
        g_perBuilderPlatformAndBuildType = {};
        buildInfo = platformAndBuildType(builder);
        var message = 'Builder: ' + builder;
        assertEquals(buildInfo.platform, expectedPlatform, message);
        assertEquals(buildInfo.buildType, expectedBuildType, message);
    }
    runPlatformAndBuildTypeTest('Webkit Win (deps)', 'XP', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Win (deps)(dbg)(1)', 'XP', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Win (deps)(dbg)(2)', 'XP', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Linux (deps)', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Linux (deps)(dbg)(1)', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Linux (deps)(dbg)(2)', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.6 (deps)', 'SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Mac10.6 (deps)(dbg)(1)', 'SNOWLEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.6 (deps)(dbg)(2)', 'SNOWLEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Win', 'XP', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Vista', 'VISTA', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Win7', 'WIN7', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Win (dbg)(1)', 'XP', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Win (dbg)(2)', 'XP', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Linux', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Linux 32', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Linux (dbg)(1)', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Linux (dbg)(2)', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.5', 'LEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Mac10.5 (dbg)(1)', 'LEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.5 (dbg)(2)', 'LEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.6', 'SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Mac10.6 (dbg)', 'SNOWLEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Chromium Win Release (Tests)', 'XP', 'RELEASE');
    runPlatformAndBuildTypeTest('Chromium Linux Release (Tests)', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Chromium Mac Release (Tests)', 'SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Win - GPU', 'XP', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Vista - GPU', 'VISTA', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Win7 - GPU', 'WIN7', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Win (dbg)(1) - GPU', 'XP', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Win (dbg)(2) - GPU', 'XP', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Linux - GPU', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Linux 32 - GPU', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Linux (dbg)(1) - GPU', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Linux (dbg)(2) - GPU', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.5 - GPU', 'LEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Mac10.5 (dbg)(1) - GPU', 'LEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.5 (dbg)(2) - GPU', 'LEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Webkit Mac10.6 - GPU', 'SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('Webkit Mac10.6 (dbg) - GPU', 'SNOWLEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Win7 Tests - GPU', 'WIN7', 'RELEASE');
    runPlatformAndBuildTypeTest('GPU Win7 Tests (dbg)(1) - GPU', 'WIN7', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Win7 Tests (dbg)(2) - GPU', 'WIN7', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Win7 x64 Tests (dbg)(1) - GPU', 'WIN7', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Win7 x64 Tests (dbg)(2) - GPU', 'WIN7', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Vista Tests (dbg)(1) - GPU', 'VISTA', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Vista Tests (dbg)(2) - GPU', 'VISTA', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Vista x64 Tests (dbg) - GPU', 'VISTA', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Mac 10.6 Tests - GPU', 'SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('GPU Mac 10.6 Tests (dbg) - GPU', 'SNOWLEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Mac 10.5 Tests (dbg) - GPU', 'LEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Linux Tests (dbg)(1) - GPU', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Linux Tests (dbg)(2) - GPU', 'LUCID', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Win7 Tests (dbg)(1) - GPU', 'WIN7', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Win7 Tests (dbg)(2) - GPU', 'WIN7', 'DEBUG');
    runPlatformAndBuildTypeTest('GPU Linux Tests x64 - GPU', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('XP Tests', 'XP', 'RELEASE');
}

function testRealModifiers()
{
    assertEquals(realModifiers('BUGFOO LINUX LEOPARD WIN DEBUG SLOW'), 'SLOW');
    assertEquals(realModifiers('BUGFOO LUCID MAC XP RELEASE SKIP'), 'SKIP');
    assertEquals(realModifiers('BUGFOO'), '');
}

function testAllTestsWithSamePlatformAndBuildType()
{
    // FIXME: test that allTestsWithSamePlatformAndBuildType actually returns the right set of tests.
    for (var i = 0; i < PLATFORMS.length; i++) {
        if (!g_allTestsByPlatformAndBuildType[PLATFORMS[i]])
            throw Error(PLATFORMS[i] + ' is not in g_allTestsByPlatformAndBuildType');
    }
}

function runTests()
{
    document.body.innerHTML = '<pre id=unittest-results></pre>';
    for (var name in window) {
        if (typeof window[name] == 'function' && /^test/.test(name)) {
            setupExpectationsTest();

            var test = window[name];
            var error = null;

            try {
                test();
            } catch (err) {
                error = err;
            }

            var result = error ? error.toString() : 'PASSED';
            $('unittest-results').insertAdjacentHTML("beforeEnd", name + ': ' + result + '\n');
        }
    }
}

if (document.readyState == 'complete')
    runTests();
else
    window.addEventListener('load', runTests, false);
