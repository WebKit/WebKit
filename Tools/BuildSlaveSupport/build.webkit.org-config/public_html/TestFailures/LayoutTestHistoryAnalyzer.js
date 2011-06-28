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
    this._loader = new LayoutTestResultsLoader(builder);

    this._currentlyFailing = {};
    this._lastSeenFailing = {};
    this._lastSeenPassing = {};
    this._possiblyFlaky = {};
    this._testRunsSinceLastChange = 0;
}

LayoutTestHistoryAnalyzer.prototype = {
    /*
     * Preiodically calls callback until all current failures have been explained. Callback is
     * passed an object like the following:
     * {
     *     'r12347 (681)': {
     *         'tooManyFailures': false,
     *         'tests': {
     *             'css1/basic/class_as_selector2.html': 'fail',
     *         },
     *     },
     *     'r12346 (680)': {
     *         'tooManyFailures': false,
     *         'tests': {},
     *     },
     *     'r12345 (679)': {
     *         'tooManyFailures': false,
     *         'tests': {
     *             'css1/basic/class_as_selector.html': 'crash',
     *         },
     *     },
     * },
     * Each build contains just the failures that a) are still occuring on the bots, and b) were new
     * in that build.
     */
    start: function(callback) {
        var self = this;
        self._builder.getBuildNames(function(buildNames) {
            function inner(buildIndex) {
                self._incorporateBuildHistory(buildNames, buildIndex, function(callAgain) {
                    var nextIndex = buildIndex + 1;
                    if (nextIndex >= buildNames.length)
                        callAgain = false;
                    callback(self._currentlyFailing, self._lastSeenFailing, self._lastSeenPassing, self._possiblyFlaky, buildNames[buildIndex], callAgain);
                    if (!callAgain)
                        return;
                    setTimeout(function() { inner(nextIndex) }, 0);
                });
            }
            inner(0);
        });
    },

    _incorporateBuildHistory: function(buildNames, buildIndex, callback) {
        var self = this;
        self._loader.start(buildNames[buildIndex], function(tests, tooManyFailures) {
            ++self._testRunsSinceLastChange;

            if (!Object.keys(self._currentlyFailing).length) {
                for (var testName in tests)
                    self._currentlyFailing[testName] = tests[testName];
            }

            for (var testName in tests) {
                if (testName in self._possiblyFlaky) {
                    self._possiblyFlaky[testName] = tests[testName];
                    continue;
                }

                if (testName in self._lastSeenPassing) {
                    delete self._lastSeenPassing[testName];
                    self._possiblyFlaky[testName] = tests[testName];
                    self._testRunsSinceLastChange = 0;
                    continue;
                }

                self._lastSeenFailing[testName] = tests[testName];
            }

            var anyOriginalTestsWereFailing = Object.keys(self._currentlyFailing).some(function(testName) { return testName in self._lastSeenFailing });

            for (var testName in self._lastSeenFailing) {
                if (testName in tests)
                    continue;
                delete self._lastSeenFailing[testName];
                self._lastSeenPassing[testName] = tests[testName];
            }

            var anyOriginalTestsAreFailing = Object.keys(self._currentlyFailing).some(function(testName) { return testName in self._lastSeenFailing });

            if (anyOriginalTestsWereFailing && !anyOriginalTestsAreFailing)
                self._testRunsSinceLastChange = 0;

            callback(anyOriginalTestsAreFailing || self._testRunsSinceLastChange < 5);
        },
        function(tests) {
            // Some tests failed, but we couldn't fetch results.html (perhaps because the test
            // run aborted early for some reason). Just skip this build entirely.
            callback(true);
        });
    },
};
