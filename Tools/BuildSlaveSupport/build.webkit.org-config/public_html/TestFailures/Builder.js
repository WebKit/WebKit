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

function Builder(name, buildbot) {
    this.name = name;
    this.buildbot = buildbot;
    this._cache = {};
}

Builder.prototype = {
    buildURL: function(buildName) {
        return this.buildbot.buildURL(this.name, buildName);
    },

    failureDiagnosisTextAndURL: function(buildName, testName, failureType) {
        var urlStem = this.resultsDirectoryURL(buildName) + testName.replace(/\.[^.]+$/, '');
        var diagnosticInfo = {
            fail: {
                text: 'pretty diff',
                url: urlStem + '-pretty-diff.html',
            },
            timeout: {
                text: 'timed out',
            },
            crash: {
                text: 'crash log',
                url: urlStem + '-crash-log.txt',
            },
            'webprocess crash': {
                text: 'web process crash log',
                url: urlStem + '-crash-log.txt',
            },
        };

        return diagnosticInfo[failureType];
    },

    /*
     * Preiodically calls callback until all current failures have been explained. Callback is
     * passed an object like the following:
     * {
     *     'r2_1 (1)': {
     *         'css1/basic/class_as_selector2.html': 'fail',
     *     },
     *     'r1_1 (0)': {
     *         'css1/basic/class_as_selector.html': 'crash',
     *     },
     * },
     * Each build contains just the failures that a) are still occuring on the bots, and b) were new
     * in that build.
     */
    startFetchingBuildHistory: function(callback) {
        var cacheKey = '_startFetchingBuildHistory';
        if (!(cacheKey in this._cache))
            this._cache[cacheKey] = {};

        var history = this._cache[cacheKey];

        var self = this;
        self._getBuildNames(function(buildNames) {
            function inner(buildIndex) {
                self._incorporateBuildHistory(buildNames, buildIndex, history, function(callAgain) {
                    callback(history);
                    if (!callAgain)
                        return;
                    var nextIndex = buildIndex + 1;
                    if (nextIndex >= buildNames.length)
                        return;
                    setTimeout(function() { inner(nextIndex) }, 0);
                });
            }
            inner(0);
        });
    },

    resultsDirectoryURL: function(buildName) {
        return this.buildbot.resultsDirectoryURL(this.name, buildName);
    },

    _getBuildNames: function(callback) {
        var cacheKey = '_getBuildNames';
        if (cacheKey in this._cache) {
            callback(this._cache[cacheKey]);
            return;
        }

        var self = this;
        getResource(this.buildbot.baseURL + 'results/' + this.name, function(xhr) {
            var root = document.createElement('html');
            root.innerHTML = xhr.responseText;

            var buildNames = Array.prototype.map.call(root.querySelectorAll('td:first-child > a > b'), function(elem) {
                return elem.innerText.replace(/\/$/, '');
            }).filter(function(filename) {
                return !/\.zip$/.test(filename);
            });
            buildNames.reverse();

            self._cache[cacheKey] = buildNames;
            callback(buildNames);
        });
    },

    _getFailingTests: function(buildName, callback, errorCallback) {
        var cacheKey = '_getFailingTests_' + buildName;
        if (cacheKey in this._cache) {
            callback(this._cache[cacheKey]);
            return;
        }

        var tests = {};
        this._cache[cacheKey] = tests;

        var self = this;
        getResource(self.buildbot.baseURL + 'json/builders/' + self.name + '/builds/' + self.buildbot.parseBuildName(buildName).buildNumber, function(xhr) {
            var data = JSON.parse(xhr.responseText);
            var layoutTestStep = data.steps.findFirst(function(step) { return step.name === 'layout-test'; });
            if (!('isStarted' in layoutTestStep)) {
                // run-webkit-tests never even ran.
                errorCallback(tests);
                return;
            }

            if (!('results' in layoutTestStep) || layoutTestStep.results[0] === 0) {
                // All tests passed.
                callback(tests);
                return;
            }

            if (/^Exiting early/.test(layoutTestStep.results[1][0])) {
                // Too many tests crashed or timed out. We can't use this test run.
                errorCallback(tests);
                return;
            }

            // Find out which tests failed.
            getResource(self.resultsDirectoryURL(buildName) + 'results.html', function(xhr) {
                var root = document.createElement('html');
                root.innerHTML = xhr.responseText;

                function testsForResultTable(regex) {
                    var paragraph = Array.prototype.findFirst.call(root.querySelectorAll('p'), function(paragraph) {
                        return regex.test(paragraph.innerText);
                    });
                    if (!paragraph)
                        return [];
                    var table = paragraph.nextElementSibling;
                    console.assert(table.nodeName === 'TABLE');
                    return Array.prototype.map.call(table.querySelectorAll('td:first-child > a'), function(elem) {
                        return elem.innerText;
                    });
                }

                testsForResultTable(/did not match expected results/).forEach(function(name) {
                    tests[name] = 'fail';
                });
                testsForResultTable(/timed out/).forEach(function(name) {
                    tests[name] = 'timeout';
                });
                testsForResultTable(/tool to crash/).forEach(function(name) {
                    tests[name] = 'crash';
                });
                testsForResultTable(/Web process to crash/).forEach(function(name) {
                    tests[name] = 'webprocess crash';
                });

                callback(tests);
            },
            function(xhr) {
                // We failed to fetch results.html. run-webkit-tests must have aborted early.
                errorCallback(tests);
            });
        });
    },

    _incorporateBuildHistory: function(buildNames, buildIndex, history, callback) {
        var previousBuildName = Object.keys(history).last();
        var nextBuildName = buildNames[buildIndex];

        this._getFailingTests(nextBuildName, function(tests) {
            history[nextBuildName] = {};

            for (var testName in tests) {
                if (previousBuildName) {
                    if (!(testName in history[previousBuildName]))
                        continue;
                    delete history[previousBuildName][testName];
                }
                history[nextBuildName][testName] = tests[testName];
            }

            callback(Object.keys(history[nextBuildName]).length);
        },
        function(tests) {
            // Some tests failed, but we couldn't fetch results.html (perhaps because the test
            // run aborted early for some reason). Just skip this build entirely.
            callback(true);
        });
    },
};
