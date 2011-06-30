/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

function LayoutTestHistoryAnalyzer(builder) {
    this._builder = builder;
    this._flakinessDetector = new FlakyLayoutTestDetector();
    this._history = {};
    this._loader = new LayoutTestResultsLoader(builder);
    this._testRunsSinceLastInterestingChange = 0;
}

LayoutTestHistoryAnalyzer.prototype = {
    /*
     * Periodically calls callback until all current failures have been explained. Callback is
     * passed an object like the following:
     * {
     *     'history': {
     *         'r12347 (681)': {
     *             'tooManyFailures': false,
     *             'tests': {
     *                 'css1/basic/class_as_selector2.html': 'fail',
     *             },
     *         },
     *         'r12346 (680)': {
     *             'tooManyFailures': false,
     *             'tests': {},
     *         },
     *         'r12345 (679)': {
     *             'tooManyFailures': false,
     *             'tests': {
     *                 'css1/basic/class_as_selector.html': 'crash',
     *             },
     *         },
     *     },
     *     'possiblyFlaky': {
     *         'fast/workers/worker-test.html': [
     *             { 'build': 'r12345 (679)', 'result': 'pass' },
     *             { 'build': 'r12344 (678)', 'result': 'fail' },
     *             { 'build': 'r12340 (676)', 'result': 'fail' },
     *             { 'build': 'r12338 (675)', 'result': 'pass' },
     *         ],
     *     },
     * }
     * Each build contains just the failures that a) are still occurring on the bots, and b) were new
     * in that build.
     */
    start: function(callback) {
        var self = this;
        self._builder.getBuildNames(function(buildNames) {
            self._analyzeBuilds(buildNames, callback, function() {
                self._builder.getOldBuildNames(function(oldBuildNames) {
                    self._analyzeBuilds(oldBuildNames, callback);
                });
            });
        });
    },

    _analyzeBuilds: function(buildNames, callback, analyzedAllBuildsCallback) {
        var self = this;
        function inner(buildIndex) {
            self._incorporateBuildHistory(buildNames, buildIndex, function(callAgain) {
                var data = {
                    history: self._history,
                    possiblyFlaky: {},
                };
                self._flakinessDetector.possiblyFlakyTests.forEach(function(testName) {
                    data.possiblyFlaky[testName] = self._flakinessDetector.flakinessExamples(testName);
                });

                var nextIndex = buildIndex + 1;
                var analyzedAllBuilds = nextIndex >= buildNames.length;
                var haveMoreDataToFetch = !analyzedAllBuilds || analyzedAllBuildsCallback;

                var callbackRequestedStop = !callback(data, haveMoreDataToFetch);
                if (callbackRequestedStop)
                    return;

                if (analyzedAllBuilds) {
                    if (analyzedAllBuildsCallback)
                        analyzedAllBuildsCallback();
                    return;
                }

                setTimeout(function() { inner(nextIndex) }, 0);
            });
        }
        inner(0);
    },

    _incorporateBuildHistory: function(buildNames, buildIndex, callback) {
        var previousBuildName = Object.keys(this._history).last();
        var nextBuildName = buildNames[buildIndex];

        var self = this;
        self._loader.start(nextBuildName, function(tests, tooManyFailures) {
            ++self._testRunsSinceLastInterestingChange;

            var historyItem = {
                tooManyFailures: tooManyFailures,
                tests: {},
            };
            self._history[nextBuildName] = historyItem;

            var previousHistoryItem;
            if (previousBuildName)
                previousHistoryItem = self._history[previousBuildName];

            var newFlakyTests = self._flakinessDetector.incorporateTestResults(nextBuildName, tests, tooManyFailures);
            if (newFlakyTests.length) {
                self._testRunsSinceLastInterestingChange = 0;
                // Remove all possibly flaky tests from the failure history, since when they failed
                // is no longer meaningful.
                newFlakyTests.forEach(function(testName) {
                    for (var buildName in self._history)
                        delete self._history[buildName].tests[testName];
                });
            }

            for (var testName in tests) {
                if (previousHistoryItem) {
                    if (!(testName in previousHistoryItem.tests))
                        continue;
                    delete previousHistoryItem.tests[testName];
                }
                historyItem.tests[testName] = tests[testName];
            }

            if (tooManyFailures && previousHistoryItem) {
                // Not all tests were run due to too many failures. Just assume that all the tests
                // that failed in the last build would still have failed in this build had they been
                // run.
                for (var testName in previousHistoryItem.tests) {
                    historyItem.tests[testName] = previousHistoryItem.tests[testName];
                    delete previousHistoryItem.tests[testName];
                }
            }

            var previousUnexplainedFailuresCount = previousBuildName ? Object.keys(self._history[previousBuildName].tests).length : 0;
            var unexplainedFailuresCount = Object.keys(self._history[nextBuildName].tests).length;

            if (previousUnexplainedFailuresCount && !unexplainedFailuresCount)
                self._testRunsSinceLastInterestingChange = 0;

            const minimumRequiredTestRunsWithoutInterestingChanges = 5;
            callback(unexplainedFailuresCount || self._testRunsSinceLastInterestingChange < minimumRequiredTestRunsWithoutInterestingChanges);
        },
        function(tests) {
            // Some tests failed, but we couldn't fetch results.html (perhaps because the test
            // run aborted early for some reason). Just skip this build entirely.
            callback(true);
        });
    },
};
