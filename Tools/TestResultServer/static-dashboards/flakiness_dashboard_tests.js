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
// In the meanwhile, you can run these tests by loading
// flakiness_dashboard.html#useTestData=true in a browser.

// Clears out the global objects modified or used by processExpectations and
// populateExpectationsData. A bit gross since it's digging into implementation
// details, but it's good enough for now.
function setupExpectationsTest()
{
    allExpectations = null;
    allTests = null;
    g_expectations = '';
    g_resultsByBuilder = {};
    g_builders = {};
    g_allExpectations = null;
    g_allTests = null;
    g_currentState = {};
    for (var key in g_defaultCrossDashboardStateValues) {
        g_currentState[key] = g_defaultCrossDashboardStateValues[key];
    }
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

function testFlattenTrie()
{
    var tests = {
        'bar.html': {'results': [[100, 'F']], 'times': [[100, 0]]},
        'foo': {
            'bar': {
                'baz.html': {'results': [[100, 'F']], 'times': [[100, 0]]},
            }
        }
    };
    var expectedFlattenedTests = {
        'bar.html': {'results': [[100, 'F']], 'times': [[100, 0]]},
        'foo/bar/baz.html': {'results': [[100, 'F']], 'times': [[100, 0]]},
    };
    assertEquals(JSON.stringify(flattenTrie(tests)), JSON.stringify(expectedFlattenedTests))
}

function testReleaseFail()
{
    var builder = 'Webkit Win';
    var test = 'foo/1.html';
    var expectationsArray = [
        {'modifiers': 'RELEASE', 'expectations': 'FAIL'}
    ];
    g_expectations = 'RELEASE : ' + test + ' = FAIL';
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
    g_expectations = 'RELEASE : ' + test + ' = FAIL\n' +
        'DEBUG : ' + test + ' = CRASH';
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
    g_expectations = 'RELEASE : ' + test + ' = FAIL\n' +
        'DEBUG : ' + test + ' = CRASH';
    runExpectationsTest(builder, test, 'CRASH', 'DEBUG');
}

function testOverrideJustBuildType()
{
    var test = 'bar/1.html';
    g_expectations = 'WONTFIX : bar = FAIL PASS TIMEOUT\n' +
        'WONTFIX MAC : ' + test + ' = FAIL\n' +
        'LINUX DEBUG : ' + test + ' = CRASH';
    
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
    runPlatformAndBuildTypeTest('Interactive Tests (dbg)', 'XP', 'DEBUG');
    
    g_currentState.group = '@ToT - webkit.org';
    runPlatformAndBuildTypeTest('Chromium Win Release (Tests)', 'XP', 'RELEASE');
    runPlatformAndBuildTypeTest('Chromium Linux Release (Tests)', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Chromium Mac Release (Tests)', 'SNOWLEOPARD', 'RELEASE');
    
    // FIXME: These platforms should match whatever we use in the test_expectations.txt format.
    runPlatformAndBuildTypeTest('Leopard Intel Release (Tests)', 'APPLE_LEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('Leopard Intel Debug (Tests)', 'APPLE_LEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('SnowLeopard Intel Release (Tests)', 'APPLE_SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('SnowLeopard Intel Leaks', 'APPLE_SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('SnowLeopard Intel Debug (Tests)', 'APPLE_SNOWLEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('GTK Linux 32-bit Release', 'GTK_LINUX', 'RELEASE');
    runPlatformAndBuildTypeTest('GTK Linux 32-bit Debug', 'GTK_LINUX', 'DEBUG');
    runPlatformAndBuildTypeTest('GTK Linux 64-bit Debug', 'GTK_LINUX', 'DEBUG');
    runPlatformAndBuildTypeTest('Qt Linux Release', 'QT_LINUX', 'RELEASE');
    runPlatformAndBuildTypeTest('Windows 7 Release (Tests)', 'APPLE_WIN7', 'RELEASE');
    runPlatformAndBuildTypeTest('Windows XP Debug (Tests)', 'APPLE_XP', 'DEBUG');
    
    // FIXME: Should WebKit2 be it's own platform?
    runPlatformAndBuildTypeTest('SnowLeopard Intel Release (WebKit2 Tests)', 'APPLE_SNOWLEOPARD', 'RELEASE');
    runPlatformAndBuildTypeTest('SnowLeopard Intel Debug (WebKit2 Tests)', 'APPLE_SNOWLEOPARD', 'DEBUG');
    runPlatformAndBuildTypeTest('Windows 7 Release (WebKit2 Tests)', 'APPLE_WIN7', 'RELEASE');    
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

function testFilterBugs()
{
    var filtered = filterBugs('SKIP BUG123 BUGCR123 BUGWK123 SLOW BUG_TONY DEBUG')
    assertEquals(filtered.modifiers, 'SKIP SLOW DEBUG');
    assertEquals(filtered.bugs, 'BUG123 BUGCR123 BUGWK123 BUG_TONY');

    filtered = filterBugs('SKIP SLOW DEBUG')
    assertEquals(filtered.modifiers, 'SKIP SLOW DEBUG');
    assertEquals(filtered.bugs, '');
}

function testGetExpectations()
{
    g_builders['WebKit Win'] = true;
    g_resultsByBuilder = {
        'WebKit Win': {
            'tests': {
                'foo/test1.html': {'results': [[100, 'F']], 'times': [[100, 0]]},
                'foo/test3.html': {'results': [[100, 'F']], 'times': [[100, 0]]},
                'test1.html': {'results': [[100, 'F']], 'times': [[100, 0]]}
            }
        }
    }

    g_expectations = 'BUG123 : foo = FAIL PASS CRASH\n' +
        'RELEASE BUGFOO : foo/test1.html = FAIL\n' +
        'DEBUG : foo/test1.html = CRASH\n' +
        'BUG456 : foo/test2.html = FAIL\n' +
        'LINUX DEBUG : foo/test2.html = CRASH\n' +
        'RELEASE : test1.html = FAIL\n' +
        'DEBUG : test1.html = CRASH\n' +
        'WIN7 : http/tests/appcache/interrupted-update.html = TIMEOUT\n' +
        'MAC LINUX XP VISTA : http/tests/appcache/interrupted-update.html = FAIL\n';

    processExpectations();
    
    var expectations = getExpectations('foo/test1.html', 'XP', 'DEBUG');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"DEBUG","expectations":"CRASH"}');

    var expectations = getExpectations('foo/test1.html', 'LUCID', 'RELEASE');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"RELEASE BUGFOO","expectations":"FAIL"}');

    var expectations = getExpectations('foo/test2.html', 'LUCID', 'RELEASE');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"BUG456","expectations":"FAIL"}');

    var expectations = getExpectations('foo/test2.html', 'LEOPARD', 'DEBUG');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"BUG456","expectations":"FAIL"}');

    var expectations = getExpectations('foo/test2.html', 'LUCID', 'DEBUG');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"LINUX DEBUG","expectations":"CRASH"}');

    var expectations = getExpectations('foo/test3.html', 'LUCID', 'DEBUG');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"BUG123","expectations":"FAIL PASS CRASH"}');

    var expectations = getExpectations('test1.html', 'XP', 'DEBUG');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"DEBUG","expectations":"CRASH"}');

    var expectations = getExpectations('test1.html', 'LUCID', 'RELEASE');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"RELEASE","expectations":"FAIL"}');

    var expectations = getExpectations('http/tests/appcache/interrupted-update.html', 'WIN7', 'RELEASE');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"WIN7","expectations":"TIMEOUT"}');

    var expectations = getExpectations('http/tests/appcache/interrupted-update.html', 'LEOPARD', 'RELEASE');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"MAC LINUX XP VISTA","expectations":"FAIL"}');

    var expectations = getExpectations('http/tests/appcache/interrupted-update.html', 'LUCID', 'RELEASE');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"MAC LINUX XP VISTA","expectations":"FAIL"}');

    var expectations = getExpectations('http/tests/appcache/interrupted-update.html', 'VISTA', 'RELEASE');
    assertEquals(JSON.stringify(expectations), '{"modifiers":"MAC LINUX XP VISTA","expectations":"FAIL"}');
}

function testSubstringList()
{
    g_currentState.testType = 'gtest';
    g_currentState.tests = 'test.FLAKY_foo test.FAILS_foo1 test.DISABLED_foo2 test.MAYBE_foo3 test.foo4';
    assertEquals(substringList().toString(), 'test.foo,test.foo1,test.foo2,test.foo3,test.foo4');

    g_currentState.testType = 'layout-tests';
    g_currentState.tests = 'foo/bar.FLAKY_foo.html';
    assertEquals(substringList().toString(), 'foo/bar.FLAKY_foo.html');
}

function testHtmlForTestsWithExpectationsButNoFailures()
{
    var builder = 'WebKit Win';
    g_perBuilderWithExpectationsButNoFailures[builder] = ['passing-test1.html', 'passing-test2.html'];
    g_perBuilderSkippedPaths[builder] = ['skipped-test1.html'];
    g_resultsByBuilder[builder] = { buildNumbers: [5, 4, 3, 1] };

    g_currentState.showUnexpectedPasses = true;
    g_currentState.showSkipped = true;
    
    var container = document.createElement('div');
    container.innerHTML = htmlForTestsWithExpectationsButNoFailures(builder);
    assertEquals(container.querySelectorAll('#passing-tests > div').length, 2);
    assertEquals(container.querySelectorAll('#skipped-tests > div').length, 1);
    
    g_currentState.showUnexpectedPasses = false;
    g_currentState.showSkipped = false;
    
    var container = document.createElement('div');
    container.innerHTML = htmlForTestsWithExpectationsButNoFailures(builder);
    assertEquals(container.querySelectorAll('#passing-tests > div').length, 0);
    assertEquals(container.querySelectorAll('#skipped-tests > div').length, 0);
}

function testHeaderForTestTableHtml()
{
    var container = document.createElement('div');
    container.innerHTML = headerForTestTableHtml();
    assertEquals(container.querySelectorAll('input').length, 5);
}

function testHtmlForTestTypeSwitcherGroup()
{
    var container = document.createElement('div');
    g_currentState.testType = 'ui_tests';
    container.innerHTML = htmlForTestTypeSwitcher();
    var selects = container.querySelectorAll('select');
    assertEquals(selects.length, 3);
    var group = selects[2];
    assertEquals(group.parentNode.textContent.indexOf('Group:'), 0);
    assertEquals(group.children.length, 2);

    g_currentState.testType = 'layout-tests';
    container.innerHTML = htmlForTestTypeSwitcher();
    var selects = container.querySelectorAll('select');
    assertEquals(selects.length, 3);
    var group = selects[2];
    assertEquals(group.parentNode.textContent.indexOf('Group:'), 0);
    assertEquals(group.children.length, 4);
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
