/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @fileoverview Poor-man's unittests for some of the trickier logic bits of
 * the flakiness dashboard.
 *
 * Currently only tests processExpectations and populateExpectationsData.
 * A test just consists of calling runExpectationsTest with the appropriate
 * arguments.
 */

// TODO(ojan): Add more test cases.
// TODO(ojan): Add tests for processMissingAndExtraExpectations

/**
 * Clears out the global objects modified or used by processExpectations and
 * populateExpectationsData. A bit gross since it's digging into implementation
 * details, but it's good enough for now.
 */
function setupExpectationsTest() {
  allExpectations = null;
  allTests = null;
  g_expectationsByTest = {};
  g_resultsByBuilder = {};
  g_builders = {};
}

/**
 * Processes the expectations for a test and asserts that the final expectations
 * and modifiers we apply to a test match what we expect.
 *
 * @param {string} builder Builder the test is run on.
 * @param {string} test The test name.
 * @param {string} expectations Sorted string of what the expectations for this
 *    test ought to be for this builder.
 * @param {string} modifiers Sorted string of what the modifiers for this
 *    test ought to be for this builder.
 */
function runExpectationsTest(builder, test, expectations, modifiers) {
  g_builders[builder] = true;

  // Put in some dummy results. processExpectations expects the test to be
  // there.
  var tests = {};
  tests[test] = {'results': [[100, 'F']], 'times': [[100, 0]]};
  g_resultsByBuilder[builder] = {'tests': tests};

  processExpectations();
  var resultsForTest = createResultsObjectForTest(test, builder);
  populateExpectationsData(resultsForTest);

  assertEquals(resultsForTest, resultsForTest.expectations, expectations);
  assertEquals(resultsForTest, resultsForTest.modifiers, modifiers);
}

function assertEquals(resultsForTest, actual, expected) {
  if (expected !== actual) {
    throw Error('Builder: ' + resultsForTest.builder + ' test: ' +
        resultsForTest.test + ' got: ' + actual + ' expected: ' + expected);
  }
}

function throwError(resultsForTests, actual, expected) {
}

function testReleaseFail() {
  var builder = 'Webkit';
  var test = 'foo/1.html';
  var expectationsArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'}
  ];
  g_expectationsByTest[test] = expectationsArray;
  runExpectationsTest(builder, test, 'FAIL', 'RELEASE');
}

function testReleaseFailDebugCrashReleaseBuilder() {
  var builder = 'Webkit';
  var test = 'foo/1.html';
  var expectationsArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
    {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
  ];
  g_expectationsByTest[test] = expectationsArray;
  runExpectationsTest(builder, test, 'FAIL', 'RELEASE');
}

function testReleaseFailDebugCrashDebugBuilder() {
  var builder = 'Webkit(dbg)';
  var test = 'foo/1.html';
  var expectationsArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
    {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
  ];
  g_expectationsByTest[test] = expectationsArray;
  runExpectationsTest(builder, test, 'CRASH', 'DEBUG');
}

function testOverrideJustBuildType() {
  var test = 'bar/1.html';
  g_expectationsByTest['bar'] = [
    {'modifiers': 'WONTFIX', 'expectations': 'FAIL PASS TIMEOUT'}
  ];
  g_expectationsByTest[test] = [
    {'modifiers': 'WONTFIX MAC', 'expectations': 'FAIL'},
    {'modifiers': 'LINUX DEBUG', 'expectations': 'CRASH'},
  ];
  runExpectationsTest('Webkit', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
  runExpectationsTest('Webkit (dbg)(3)', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
  runExpectationsTest('Webkit Linux', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
  runExpectationsTest('Webkit Linux (dbg)(3)', test, 'CRASH', 'LINUX DEBUG');
  runExpectationsTest('Webkit Mac10.5', test, 'FAIL', 'WONTFIX MAC');
  runExpectationsTest('Webkit Mac10.5 (dbg)(3)', test, 'FAIL', 'WONTFIX MAC');
}

function runTests() {
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
      $('unittest-results').insertAdjacentHTML("beforeEnd",
          name + ': ' + result + '\n');
    }
  }
}

if (document.readyState == 'complete') {
  runTests();
} else {
  window.addEventListener('load', runTests, false);
}
