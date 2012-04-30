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
    runPlatformAndBuildTypeTest('XP Tests', 'XP', 'RELEASE');
    runPlatformAndBuildTypeTest('Interactive Tests (dbg)', 'XP', 'DEBUG');
    
    g_crossDashboardState.group = '@ToT - webkit.org';
    g_crossDashboardState.testType = 'layout-tests';
    runPlatformAndBuildTypeTest('Chromium Win Release (Tests)', 'XP', 'RELEASE');
    runPlatformAndBuildTypeTest('Chromium Linux Release (Tests)', 'LUCID', 'RELEASE');
    runPlatformAndBuildTypeTest('Chromium Mac Release (Tests)', 'SNOWLEOPARD', 'RELEASE');
    
    // FIXME: These platforms should match whatever we use in the test_expectations.txt format.
    runPlatformAndBuildTypeTest('Lion Release (Tests)', 'APPLE_LION', 'RELEASE');
    runPlatformAndBuildTypeTest('Lion Debug (Tests)', 'APPLE_LION', 'DEBUG');
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
    g_crossDashboardState.testType = 'gtest';
    g_currentState.tests = 'test.FLAKY_foo test.FAILS_foo1 test.DISABLED_foo2 test.MAYBE_foo3 test.foo4';
    assertEquals(substringList().toString(), 'test.foo,test.foo1,test.foo2,test.foo3,test.foo4');

    g_crossDashboardState.testType = 'layout-tests';
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

    g_crossDashboardState.group = '@ToT - chromium.org';
    g_crossDashboardState.testType = 'layout-tests';
    
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
    g_crossDashboardState.testType = 'ui_tests';
    container.innerHTML = htmlForTestTypeSwitcher();
    var selects = container.querySelectorAll('select');
    assertEquals(selects.length, 3);
    var group = selects[2];
    assertEquals(group.parentNode.textContent.indexOf('Group:'), 0);
    assertEquals(group.children.length, 3);

    g_crossDashboardState.testType = 'layout-tests';
    container.innerHTML = htmlForTestTypeSwitcher();
    var selects = container.querySelectorAll('select');
    assertEquals(selects.length, 3);
    var group = selects[2];
    assertEquals(group.parentNode.textContent.indexOf('Group:'), 0);
    assertEquals(group.children.length, 3);
}

function testHtmlForIndividualTestOnAllBuilders()
{
    assertEquals(htmlForIndividualTestOnAllBuilders('foo/nonexistant.html'), '<div class="not-found">Test not found. Either it does not exist, is skipped or passes on all platforms.</div>');
}

function testHtmlForIndividualTestOnAllBuildersWithChromeNonexistant()
{
    assertEquals(htmlForIndividualTestOnAllBuildersWithChrome('foo/nonexistant.html'),
        '<h2><a href="http://trac.webkit.org/browser/trunk/LayoutTests/foo/nonexistant.html" target="_blank">foo/nonexistant.html</a></h2>' +
        '<div class="not-found">Test not found. Either it does not exist, is skipped or passes on all platforms.</div>' +
        '<div class=expectations test=foo/nonexistant.html>' +
            '<div>' +
                '<span class=link onclick="setQueryParameter(\'showExpectations\', true)">Show results</span> | ' +
                '<span class=link onclick="setQueryParameter(\'showLargeExpectations\', true)">Show large thumbnails</span> | ' +
                '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b>' +
            '</div>' +
        '</div>');
}

function testHtmlForIndividualTestOnAllBuildersWithChrome()
{
    var test = 'dummytest.html';
    var builderName = 'dummyBuilder';
    g_testToResultsMap[test] = [createResultsObjectForTest(test, builderName)];
    currentBuilderGroup().builders = {'Webkit Linux': '', 'Webkit Linux (dbg)': '', 'Webkit Mac10.5': '', 'Webkit Win': ''};

    assertEquals(htmlForIndividualTestOnAllBuildersWithChrome(test),
        '<h2><a href="http://trac.webkit.org/browser/trunk/LayoutTests/dummytest.html" target="_blank">dummytest.html</a></h2>' +
        '<table class=test-table><thead><tr>' +
                '<th sortValue=test><div class=table-header-content><span></span><span class=header-text>test</span></div></th>' +
                '<th sortValue=bugs><div class=table-header-content><span></span><span class=header-text>bugs</span></div></th>' +
                '<th sortValue=modifiers><div class=table-header-content><span></span><span class=header-text>modifiers</span></div></th>' +
                '<th sortValue=expectations><div class=table-header-content><span></span><span class=header-text>expectations</span></div></th>' +
                '<th sortValue=slowest><div class=table-header-content><span></span><span class=header-text>slowest run</span></div></th>' +
                '<th sortValue=%><div class=table-header-content><span></span><span class=header-text>% fail</span></div></th>' +
                '<th sortValue=flakiness colspan=10000><div class=table-header-content><span></span><span class=header-text>flakiness (numbers are runtimes in seconds)</span></div></th>' +
            '</tr></thead>' +
            '<tbody></tbody>' +
        '</table>' +
        '<div>The following builders either don\'t run this test (e.g. it\'s skipped) or all runs passed:</div>' +
        '<div class=skipped-builder-list>' +
            '<div class=skipped-builder>Webkit Linux</div>' +
            '<div class=skipped-builder>Webkit Linux (dbg)</div>' +
            '<div class=skipped-builder>Webkit Mac10.5</div>' +
            '<div class=skipped-builder>Webkit Win</div>' +
        '</div>' +
        '<div class=expectations test=dummytest.html>' +
            '<div><span class=link onclick="setQueryParameter(\'showExpectations\', true)">Show results</span> | ' +
            '<span class=link onclick="setQueryParameter(\'showLargeExpectations\', true)">Show large thumbnails</span> | ' +
            '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b></div>' +
        '</div>');
}

function testHtmlForIndividualTestOnAllBuildersWithChromeWebkitMaster()
{
    var test = 'dummytest.html';
    var builderName = 'dummyBuilder';
    BUILDER_TO_MASTER[builderName] = WEBKIT_BUILDER_MASTER;
    g_testToResultsMap[test] = [createResultsObjectForTest(test, builderName)];
        currentBuilderGroup().builders = {'Webkit Linux': '', 'Webkit Linux (dbg)': '', 'Webkit Mac10.5': '', 'Webkit Win': ''};

    assertEquals(htmlForIndividualTestOnAllBuildersWithChrome(test),
        '<h2><a href="http://trac.webkit.org/browser/trunk/LayoutTests/dummytest.html" target="_blank">dummytest.html</a></h2>' +
            '<table class=test-table><thead><tr>' +
                    '<th sortValue=test><div class=table-header-content><span></span><span class=header-text>test</span></div></th>' +
                    '<th sortValue=bugs><div class=table-header-content><span></span><span class=header-text>bugs</span></div></th>' +
                    '<th sortValue=modifiers><div class=table-header-content><span></span><span class=header-text>modifiers</span></div></th>' +
                    '<th sortValue=expectations><div class=table-header-content><span></span><span class=header-text>expectations</span></div></th>' +
                    '<th sortValue=slowest><div class=table-header-content><span></span><span class=header-text>slowest run</span></div></th>' +
                    '<th sortValue=%><div class=table-header-content><span></span><span class=header-text>% fail</span></div></th>' +
                    '<th sortValue=flakiness colspan=10000><div class=table-header-content><span></span><span class=header-text>flakiness (numbers are runtimes in seconds)</span></div></th>' +
                '</tr></thead>' +
                '<tbody></tbody>' +
            '</table>' +
            '<div>The following builders either don\'t run this test (e.g. it\'s skipped) or all runs passed:</div>' +
            '<div class=skipped-builder-list>' +
                '<div class=skipped-builder>Webkit Linux</div>' +
                '<div class=skipped-builder>Webkit Linux (dbg)</div>' +
                '<div class=skipped-builder>Webkit Mac10.5</div>' +
                '<div class=skipped-builder>Webkit Win</div>' +
            '</div>' +
            '<div class=expectations test=dummytest.html>' +
                '<div><span class=link onclick="setQueryParameter(\'showExpectations\', true)">Show results</span> | ' +
                '<span class=link onclick="setQueryParameter(\'showLargeExpectations\', true)">Show large thumbnails</span>' +
                '<form onsubmit="setQueryParameter(\'revision\', revision.value);return false;">' +
                    'Show results for WebKit revision: <input name=revision placeholder="e.g. 65540" value="" id=revision-input>' +
                '</form></div>' +
            '</div>');
}

function testHtmlForIndividualTests()
{
    g_currentState.showChrome = false;
    var test1 = 'foo/nonexistant.html';
    var test2 = 'bar/nonexistant.html';
    var tests = [test1, test2];
    assertEquals(htmlForIndividualTests(tests), htmlForIndividualTestOnAllBuilders(test1) + '<hr>' + htmlForIndividualTestOnAllBuilders(test2));

    g_currentState.showChrome = true;
    assertEquals(htmlForIndividualTests(tests), htmlForIndividualTestOnAllBuildersWithChrome(test1) + '<hr>' + htmlForIndividualTestOnAllBuildersWithChrome(test2));
}


function testLookupVirtualTestSuite()
{
    assertEquals(lookupVirtualTestSuite('fast/canvas/foo.html'), '');
    assertEquals(lookupVirtualTestSuite('platform/chromium/virtual/gpu/fast/canvas/foo.html'),
                      'platform/chromium/virtual/gpu/fast/canvas');
}

function testBaseTest()
{
    assertEquals(baseTest('fast/canvas/foo.html', ''), 'fast/canvas/foo.html');
    assertEquals(baseTest('platform/chromium/virtual/gpu/fast/canvas/foo.html', 'platform/chromium/virtual/gpu/fast/canvas'), 'fast/canvas/foo.html');
}

// FIXME: Create builders_tests.js and move this there.
function generateBuildersFromBuilderListHelper(builderList, expectedBuilders, builderFilter)
{
    var builders = generateBuildersFromBuilderList(builderList, builderFilter);
    assertEquals(builders.length, expectedBuilders.length);
    builders.forEach(function(builder, index) {
        var expected = expectedBuilders[index];
        assertEquals(builder.length, expected.length);
        for (var i = 0; i < builder.length; i++)
            assertEquals(builder[i], expected[i]);
    })
}

function testGenerateChromiumDepsFyiGpuBuildersFromBuilderList()
{
    var builderList = ["Linux Audio", "Linux Release (ATI)", "Linux Release (Intel)", "Mac Release (ATI)", "Win7 Audio", "Win7 Release (ATI)", "Win7 Release (Intel)", "WinXP Debug (NVIDIA)", "WinXP Release (NVIDIA)"];
    var expectedBuilders = [["Linux Release (ATI)", 2], ["Linux Release (Intel)"], ["Mac Release (ATI)"], ["Win7 Release (ATI)"], ["Win7 Release (Intel)"], ["WinXP Debug (NVIDIA)"], ["WinXP Release (NVIDIA)"] ];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isChromiumDepsFyiGpuTestRunner);
}

function testGenerateChromiumTipOfTreeGpuBuildersFromBuilderList()
{
    var builderList = ["Chrome Frame Tests", "GPU Linux (NVIDIA)", "GPU Linux (dbg) (NVIDIA)", "GPU Mac", "GPU Mac (dbg)", "GPU Win7 (NVIDIA)", "GPU Win7 (dbg) (NVIDIA)", "Linux Perf",
        "Linux Tests", "Linux Valgrind", "Mac Builder (dbg)", "Mac10.6 Perf", "Mac10.6 Tests", "Vista Perf", "Vista Tests", "Webkit Linux", "Webkit Linux (dbg)", "Webkit Linux (deps)",
        "Webkit Linux 32", "Webkit Mac Builder", "Webkit Mac Builder (dbg)", "Webkit Mac Builder (deps)", "Webkit Mac10.5", "Webkit Mac10.5 (dbg)(1)", "Webkit Mac10.5 (dbg)(2)",
        "Webkit Mac10.6", "Webkit Mac10.6 (dbg)", "Webkit Mac10.6 (deps)", "Webkit Mac10.7", "Webkit Vista", "Webkit Win", "Webkit Win (dbg)(1)", "Webkit Win (dbg)(2)",
        "Webkit Win (deps)", "Webkit Win Builder", "Webkit Win Builder (dbg)", "Webkit Win Builder (deps)", "Webkit Win7", "Win (dbg)", "Win Builder"];
    var expectedBuilders = [["GPU Linux (NVIDIA)", 2], ["GPU Linux (dbg) (NVIDIA)"], ["GPU Mac"], ["GPU Mac (dbg)"], ["GPU Win7 (NVIDIA)"], ["GPU Win7 (dbg) (NVIDIA)"]];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isChromiumTipOfTreeGpuTestRunner);
}

function testGenerateWebkitBuildersFromBuilderList()
{
    var builderList = ["Chromium Android Release", "Chromium Linux Release", "Chromium Linux Release (Grid Layout)", "Chromium Linux Release (Perf)", "Chromium Linux Release (Tests)",
        "Chromium Mac Release", "Chromium Mac Release (Perf)", "Chromium Mac Release (Tests)", "Chromium Win Release", "Chromium Win Release (Perf)", "Chromium Win Release (Tests)",
        "EFL Linux Release", "GTK Linux 32-bit Release", "GTK Linux 64-bit Debug", "GTK Linux 64-bit Release", "Lion Debug (Build)", "Lion Debug (Tests)", "Lion Debug (WebKit2 Tests)",
        "Lion Leaks", "Lion Release (Build)", "Lion Release (Perf)", "Lion Release (Tests)", "Lion Release (WebKit2 Tests)", "Qt Linux 64-bit Release (Perf)",
        "Qt Linux 64-bit Release (WebKit2 Perf)", "Qt Linux ARMv7 Release", "Qt Linux MIPS Release", "Qt Linux Release", "Qt Linux Release minimal", "Qt Linux SH4 Release",
        "Qt SnowLeopard Release", "Qt Windows 32-bit Debug", "Qt Windows 32-bit Release", "SnowLeopard Intel Debug (Build)", "SnowLeopard Intel Debug (Tests)",
        "SnowLeopard Intel Debug (WebKit2 Tests)", "SnowLeopard Intel Release (Build)", "SnowLeopard Intel Release (Tests)", "SnowLeopard Intel Release (WebKit2 Tests)",
        "WinCE Release (Build)", "WinCairo Release", "Windows 7 Release (Tests)", "Windows 7 Release (WebKit2 Tests)", "Windows Debug (Build)", "Windows Release (Build)", "Windows XP Debug (Tests)"];
    var expectedBuilders = [["Chromium Linux Release (Tests)", 2], ["Chromium Mac Release (Tests)"], ["Chromium Win Release (Tests)"], ["GTK Linux 32-bit Release"], ["GTK Linux 64-bit Debug"],
        ["GTK Linux 64-bit Release"], ["Lion Debug (Tests)"], ["Lion Debug (WebKit2 Tests)"], ["Lion Release (Tests)"], ["Lion Release (WebKit2 Tests)"], ["Qt Linux Release"],
        ["SnowLeopard Intel Debug (Tests)"], ["SnowLeopard Intel Debug (WebKit2 Tests)"], ["SnowLeopard Intel Release (Tests)"], ["SnowLeopard Intel Release (WebKit2 Tests)"]];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isWebkitTestRunner);
}

function testGenerateChromiumWebkitTipOfTreeBuildersFromBuilderList()
{
    var builderList = ["Chrome Frame Tests", "GPU Linux (NVIDIA)", "GPU Linux (dbg) (NVIDIA)", "GPU Mac", "GPU Mac (dbg)", "GPU Win7 (NVIDIA)", "GPU Win7 (dbg) (NVIDIA)", "Linux Perf", "Linux Tests",
        "Linux Valgrind", "Mac Builder (dbg)", "Mac10.6 Perf", "Mac10.6 Tests", "Vista Perf", "Vista Tests", "Webkit Linux", "Webkit Linux (dbg)", "Webkit Linux (deps)", "Webkit Linux 32",
        "Webkit Mac Builder", "Webkit Mac Builder (dbg)", "Webkit Mac Builder (deps)", "Webkit Mac10.5", "Webkit Mac10.5 (dbg)(1)", "Webkit Mac10.5 (dbg)(2)", "Webkit Mac10.6", "Webkit Mac10.6 (dbg)",
        "Webkit Mac10.6 (deps)", "Webkit Mac10.7", "Webkit Vista", "Webkit Win", "Webkit Win (dbg)(1)", "Webkit Win (dbg)(2)", "Webkit Win (deps)", "Webkit Win Builder", "Webkit Win Builder (dbg)",
        "Webkit Win Builder (deps)", "Webkit Win7", "Win (dbg)", "Win Builder"];
    var expectedBuilders = [["Webkit Linux", 2], ["Webkit Linux (dbg)"], ["Webkit Linux 32"], ["Webkit Mac10.5"], ["Webkit Mac10.5 (dbg)(1)"], ["Webkit Mac10.5 (dbg)(2)"], ["Webkit Mac10.6"],
        ["Webkit Mac10.6 (dbg)"], ["Webkit Mac10.7"], ["Webkit Vista"], ["Webkit Win"], ["Webkit Win (dbg)(1)"], ["Webkit Win (dbg)(2)"], ["Webkit Win7"]];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isChromiumWebkitTipOfTreeTestRunner);
}

function testGenerateChromiumWebkitDepsBuildersFromBuilderList()
{
    var builderList = ["Chrome Frame Tests", "GPU Linux (NVIDIA)", "GPU Linux (dbg) (NVIDIA)", "GPU Mac", "GPU Mac (dbg)", "GPU Win7 (NVIDIA)", "GPU Win7 (dbg) (NVIDIA)", "Linux Perf", "Linux Tests",
        "Linux Valgrind", "Mac Builder (dbg)", "Mac10.6 Perf", "Mac10.6 Tests", "Vista Perf", "Vista Tests", "Webkit Linux", "Webkit Linux (dbg)", "Webkit Linux (deps)", "Webkit Linux 32",
        "Webkit Mac Builder", "Webkit Mac Builder (dbg)", "Webkit Mac Builder (deps)", "Webkit Mac10.5", "Webkit Mac10.5 (dbg)(1)", "Webkit Mac10.5 (dbg)(2)", "Webkit Mac10.6", "Webkit Mac10.6 (dbg)",
        "Webkit Mac10.6 (deps)", "Webkit Mac10.7", "Webkit Vista", "Webkit Win", "Webkit Win (dbg)(1)", "Webkit Win (dbg)(2)", "Webkit Win (deps)", "Webkit Win Builder", "Webkit Win Builder (dbg)",
        "Webkit Win Builder (deps)", "Webkit Win7", "Win (dbg)", "Win Builder"];
    var expectedBuilders = [["Webkit Linux (deps)", 2], ["Webkit Mac10.6 (deps)"], ["Webkit Win (deps)"]];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isChromiumWebkitDepsTestRunner);
}

function testGenerateChromiumDepsGTestBuildersFromBuilderList()
{
    var builderList = ["Android Builder", "Chrome Frame Tests (ie6)", "Chrome Frame Tests (ie7)", "Chrome Frame Tests (ie8)", "Interactive Tests (dbg)", "Linux", "Linux Builder (dbg)",
        "Linux Builder (dbg)(shared)", "Linux Builder x64", "Linux Clang (dbg)", "Linux Sync", "Linux Tests (dbg)(1)", "Linux Tests (dbg)(2)", "Linux Tests (dbg)(shared)", "Linux Tests x64",
        "Linux x64", "Mac", "Mac 10.5 Tests (dbg)(1)", "Mac 10.5 Tests (dbg)(2)", "Mac 10.5 Tests (dbg)(3)", "Mac 10.5 Tests (dbg)(4)", "Mac 10.6 Tests (dbg)(1)", "Mac 10.6 Tests (dbg)(2)",
        "Mac 10.6 Tests (dbg)(3)", "Mac 10.6 Tests (dbg)(4)", "Mac Builder", "Mac Builder (dbg)", "Mac10.5 Tests (1)", "Mac10.5 Tests (2)", "Mac10.5 Tests (3)", "Mac10.6 Sync",
        "Mac10.6 Tests (1)", "Mac10.6 Tests (2)", "Mac10.6 Tests (3)", "NACL Tests", "NACL Tests (x64)", "Vista Tests (1)", "Vista Tests (2)", "Vista Tests (3)", "Win", "Win Aura",
        "Win Builder", "Win Builder (dbg)", "Win Builder 2010 (dbg)", "Win7 Sync", "Win7 Tests (1)", "Win7 Tests (2)", "Win7 Tests (3)", "Win7 Tests (dbg)(1)", "Win7 Tests (dbg)(2)",
        "Win7 Tests (dbg)(3)", "Win7 Tests (dbg)(4)", "Win7 Tests (dbg)(5)", "Win7 Tests (dbg)(6)", "XP Tests (1)", "XP Tests (2)", "XP Tests (3)", "XP Tests (dbg)(1)", "XP Tests (dbg)(2)",
        "XP Tests (dbg)(3)", "XP Tests (dbg)(4)", "XP Tests (dbg)(5)", "XP Tests (dbg)(6)"];
    var expectedBuilders = [["Interactive Tests (dbg)", 2], ["Linux Tests (dbg)(1)"], ["Linux Tests (dbg)(2)"], ["Linux Tests (dbg)(shared)"], ["Linux Tests x64"], ["Mac 10.5 Tests (dbg)(1)"],
        ["Mac 10.5 Tests (dbg)(2)"], ["Mac 10.5 Tests (dbg)(3)"], ["Mac 10.5 Tests (dbg)(4)"], ["Mac 10.6 Tests (dbg)(1)"], ["Mac 10.6 Tests (dbg)(2)"], ["Mac 10.6 Tests (dbg)(3)"],
        ["Mac 10.6 Tests (dbg)(4)"], ["Mac10.5 Tests (1)"], ["Mac10.5 Tests (2)"], ["Mac10.5 Tests (3)"], ["Mac10.6 Tests (1)"], ["Mac10.6 Tests (2)"], ["Mac10.6 Tests (3)"], ["NACL Tests"],
        ["NACL Tests (x64)"], ["Vista Tests (1)"], ["Vista Tests (2)"], ["Vista Tests (3)"], ["Win7 Tests (1)"], ["Win7 Tests (2)"], ["Win7 Tests (3)"], ["Win7 Tests (dbg)(1)"],
        ["Win7 Tests (dbg)(2)"], ["Win7 Tests (dbg)(3)"], ["Win7 Tests (dbg)(4)"], ["Win7 Tests (dbg)(5)"], ["Win7 Tests (dbg)(6)"], ["XP Tests (1)"], ["XP Tests (2)"], ["XP Tests (3)"],
        ["XP Tests (dbg)(1)"], ["XP Tests (dbg)(2)"], ["XP Tests (dbg)(3)"], ["XP Tests (dbg)(4)"], ["XP Tests (dbg)(5)"], ["XP Tests (dbg)(6)"]];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isChromiumDepsGTestRunner);
}

function testGenerateChromiumDepsCrosGTestBuildersFromBuilderList()
{
    var builderList = ["ChromiumOS (amd64)", "ChromiumOS (arm)", "ChromiumOS (tegra2)", "ChromiumOS (x86)", "Linux ChromiumOS (Clang dbg)", "Linux ChromiumOS Builder", "Linux ChromiumOS Builder (dbg)",
        "Linux ChromiumOS Tests (1)", "Linux ChromiumOS Tests (2)", "Linux ChromiumOS Tests (dbg)(1)", "Linux ChromiumOS Tests (dbg)(2)", "Linux ChromiumOS Tests (dbg)(3)"];
    var expectedBuilders = [["Linux ChromiumOS Tests (1)", 2], ["Linux ChromiumOS Tests (2)"], ["Linux ChromiumOS Tests (dbg)(1)"], ["Linux ChromiumOS Tests (dbg)(2)"], ["Linux ChromiumOS Tests (dbg)(3)"]];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isChromiumDepsCrosGTestRunner);
}

function testGenerateChromiumTipOfTreeGTestBuildersFromBuilderList()
{
    var builderList = ["Chrome Frame Tests", "GPU Linux (NVIDIA)", "GPU Linux (dbg) (NVIDIA)", "GPU Mac", "GPU Mac (dbg)", "GPU Win7 (NVIDIA)", "GPU Win7 (dbg) (NVIDIA)", "Linux Perf",
        "Linux Tests", "Linux Valgrind", "Mac Builder (dbg)", "Mac10.6 Perf", "Mac10.6 Tests", "Vista Perf", "Vista Tests", "Webkit Linux", "Webkit Linux (dbg)", "Webkit Linux (deps)",
        "Webkit Linux 32", "Webkit Mac Builder", "Webkit Mac Builder (dbg)", "Webkit Mac Builder (deps)", "Webkit Mac10.5", "Webkit Mac10.5 (dbg)(1)", "Webkit Mac10.5 (dbg)(2)",
        "Webkit Mac10.6", "Webkit Mac10.6 (dbg)", "Webkit Mac10.6 (deps)", "Webkit Mac10.7", "Webkit Vista", "Webkit Win", "Webkit Win (dbg)(1)", "Webkit Win (dbg)(2)",
        "Webkit Win (deps)", "Webkit Win Builder", "Webkit Win Builder (dbg)", "Webkit Win Builder (deps)", "Webkit Win7", "Win (dbg)", "Win Builder"];
    var expectedBuilders = [['Linux Tests', BuilderGroup.DEFAULT_BUILDER], ['Mac10.6 Tests'], ['Vista Tests'], ['Win (dbg)']];
    generateBuildersFromBuilderListHelper(builderList, expectedBuilders, isChromiumTipOfTreeGTestRunner);
}

function assertObjectsDeepEqual(a, b)
{
    assertEquals(Object.keys(a).length, Object.keys(b).length);
    for (var key in a)
        assertEquals(a[key], b[key]);
}

function testQueryHashAsMap()
{
    assertEquals(window.location.hash, '#useTestData=true');
    assertObjectsDeepEqual(queryHashAsMap(), {useTestData: 'true'});
}

function testParseCrossDashboardParameters()
{
    assertEquals(window.location.hash, '#useTestData=true');
    parseCrossDashboardParameters();

    var expectedParameters = {};
    for (var key in g_defaultCrossDashboardStateValues)
        expectedParameters[key] = g_defaultCrossDashboardStateValues[key];
    expectedParameters.useTestData = true;

    assertObjectsDeepEqual(g_crossDashboardState, expectedParameters);
}

function testDiffStates()
{
    var newState = {a: 1, b: 2};
    assertObjectsDeepEqual(diffStates(null, newState), newState);

    var oldState = {a: 1};
    assertObjectsDeepEqual(diffStates(oldState, newState), {b: 2});

    // FIXME: This is kind of weird. I think the existing users of this code work correctly, but it's a confusing result.
    var oldState = {c: 1};
    assertObjectsDeepEqual(diffStates(oldState, newState), {a:1, b: 2});

    var oldState = {a: 1, b: 2};
    assertObjectsDeepEqual(diffStates(oldState, newState), {});

    var oldState = {a: 2, b: 3};
    assertObjectsDeepEqual(diffStates(oldState, newState), {a: 1, b: 2});
}

function testAddBuilderLoadErrors()
{
    clearErrors();
    g_hasDoneInitialPageGeneration = false;
    g_buildersThatFailedToLoad = ['builder1', 'builder2'];
    g_staleBuilders = ['staleBuilder1'];
    addBuilderLoadErrors();
    assertEquals(g_errorMessages, 'ERROR: Failed to get data from builder1,builder2.<br>ERROR: Data from staleBuilder1 is more than 1 day stale.<br>');
}

function testBuilderGroupIsToTWebKitAttribute()
{
    var dummyMaster = new BuilderMaster('dummy.org', 'http://build.dummy.org');
    var testBuilderGroups = {
        '@ToT - dummy.org': null,
        '@DEPS - dummy.org': null,
    }
    var testJSONData = "{ \"Dummy Builder 1\": null, \"Dummy Builder 2\": null }";

    // Override g_handleBuildersListLoaded to avoid entering an infinite recursion
    // as the builder lists are being loaded with dummy data.
    g_handleBuildersListLoaded = function() { }

    onBuilderListLoad(testBuilderGroups,  function() { return true; }, dummyMaster, '@ToT - dummy.org', BuilderGroup.TOT_WEBKIT, JSON.parse(testJSONData));
    assertEquals(testBuilderGroups['@ToT - dummy.org'].isToTWebKit, true);

    onBuilderListLoad(testBuilderGroups,  function() { return true; }, dummyMaster, '@DEPS - dummy.org', BuilderGroup.DEPS_WEBKIT, JSON.parse(testJSONData));
    assertEquals(testBuilderGroups['@DEPS - dummy.org'].isToTWebKit, false);
}

function htmlEscape(string)
{
    var div = document.createElement('div');
    div.textContent = string;
    return div.innerHTML;
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

            var result = error ? htmlEscape(error.toString()) : 'PASSED';

            $('unittest-results').insertAdjacentHTML("beforeEnd", name + ': ' + result + '\n');
        }
    }
}

if (document.readyState == 'complete')
    runTests();
else
    window.addEventListener('load', runTests, false);
