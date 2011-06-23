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

    getMostRecentCompletedBuildNumber: function(callback) {
        var cacheKey = 'getMostRecentCompletedBuildNumber';
        if (cacheKey in this._cache) {
            callback(this._cache[cacheKey]);
            return;
        }

        var self = this;
        getResource(self.buildbot.baseURL + 'json/builders/' + self.name, function(xhr) {
            var data = JSON.parse(xhr.responseText);

            var currentBuilds = {};
            if ('currentBuilds' in data)
                data.currentBuilds.forEach(function(buildNumber) { currentBuilds[buildNumber] = true });

            for (var i = data.cachedBuilds.length - 1; i >= 0; --i) {
                if (data.cachedBuilds[i] in currentBuilds)
                    continue;

                self._cache[cacheKey] = data.cachedBuilds[i];
                callback(data.cachedBuilds[i]);
                return;
            }

            self._cache[cacheKey] = -1;
            callback(self._cache[cacheKey]);
        },
        function(xhr) {
            self._cache[cacheKey] = -1;
            callback(self._cache[cacheKey]);
        });
    },

    getNumberOfFailingTests: function(buildNumber, callback) {
        var cacheKey = this.name + '_getNumberOfFailingTests_' + buildNumber;
        if (PersistentCache.contains(cacheKey)) {
            callback(PersistentCache.get(cacheKey));
            return;
        }

        var self = this;
        self._getBuildJSON(buildNumber, function(data) {
            var layoutTestStep = data.steps.findFirst(function(step) { return step.name === 'layout-test'; });
            if (!layoutTestStep) {
                PersistentCache.set(cacheKey, -1);
                callback(PersistentCache.get(cacheKey), false);
                return;
            }

            if (!('isStarted' in layoutTestStep)) {
                // run-webkit-tests never even ran.
                PersistentCache.set(cacheKey, -1);
                callback(PersistentCache.get(cacheKey), false);
                return;
            }

            if (!('results' in layoutTestStep) || layoutTestStep.results[0] === 0) {
                // All tests passed.
                PersistentCache.set(cacheKey, -1);
                callback(PersistentCache.get(cacheKey), false);
                return;
            }

            var tooManyFailures = false;
            if (/^Exiting early/.test(layoutTestStep.results[1][0]))
                tooManyFailures = true;

            var failureCount = layoutTestStep.results[1].reduce(function(sum, outputLine) {
                var match = /^(\d+) test case/.exec(outputLine);
                if (!match)
                    return sum;
                // Don't count new tests as failures.
                if (outputLine.indexOf('were new') >= 0)
                    return sum;
                return sum + parseInt(match[1], 10);
            }, 0);

            PersistentCache.set(cacheKey, failureCount);
            callback(failureCount, tooManyFailures);
        });
    },

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
    startFetchingBuildHistory: function(callback) {
        var cacheKey = '_startFetchingBuildHistory';
        if (!(cacheKey in this._cache))
            this._cache[cacheKey] = {};

        var history = this._cache[cacheKey];

        var self = this;
        self._getBuildNames(function(buildNames) {
            function inner(buildIndex) {
                self._incorporateBuildHistory(buildNames, buildIndex, history, function(callAgain) {
                    var nextIndex = buildIndex + 1;
                    if (nextIndex >= buildNames.length)
                        callAgain = false;
                    callback(history, callAgain);
                    if (!callAgain)
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

    resultsPageURL: function(buildName) {
        return this.resultsDirectoryURL(buildName) + 'results.html';
    },

    _getBuildJSON: function(buildNumber, callback) {
        var cacheKey = 'getBuildJSON_' + buildNumber;
        if (cacheKey in this._cache) {
            callback(this._cache[cacheKey]);
            return;
        }

        var self = this;
        getResource(self.buildbot.baseURL + 'json/builders/' + self.name + '/builds/' + buildNumber, function(xhr) {
            var data = JSON.parse(xhr.responseText);
            self._cache[cacheKey] = data;
            callback(data);
        });
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
        var cacheKey = this.name + '__getFailingTests_' + buildName;
        if (PersistentCache.contains(cacheKey)) {
            callback(PersistentCache.get(cacheKey));
            return;
        }

        var tests = {};

        var parsedBuildName = this.buildbot.parseBuildName(buildName);

        // http://webkit.org/b/62380 was fixed in r89610.
        var resultsHTMLSupportsTooManyFailuresInfo = parsedBuildName.revision >= 89610;

        var self = this;

        function fetchAndParseResultsHTMLAndCallCallback(callback, tooManyFailures) {
            getResource(self.resultsPageURL(buildName), function(xhr) {
                var root = document.createElement('html');
                root.innerHTML = xhr.responseText;

                if (resultsHTMLSupportsTooManyFailuresInfo)
                    tooManyFailures = root.getElementsByClassName('stopped-running-early-message').length > 0;

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

                PersistentCache.set(cacheKey, tests);
                callback(tests, tooManyFailures);
            },
            function(xhr) {
                // We failed to fetch results.html. run-webkit-tests must have aborted early.
                PersistentCache.set(cacheKey, tests);
                errorCallback(tests, tooManyFailures);
            });
        }

        if (resultsHTMLSupportsTooManyFailuresInfo) {
            fetchAndParseResultsHTMLAndCallCallback(callback, false);
            return;
        }

        self.getNumberOfFailingTests(parsedBuildName.buildNumber, function(failingTestCount, tooManyFailures) {
            if (failingTestCount < 0) {
                // The number of failing tests couldn't be determined.
                PersistentCache.set(cacheKey, tests);
                errorCallback(tests, tooManyFailures);
                return;
            }

            if (!failingTestCount) {
                // All tests passed.
                PersistentCache.set(cacheKey, tests);
                callback(tests, tooManyFailures);
                return;
            }

            // Find out which tests failed.
            fetchAndParseResultsHTMLAndCallCallback(callback, tooManyFailures);
        });
    },

    _incorporateBuildHistory: function(buildNames, buildIndex, history, callback) {
        var previousBuildName = Object.keys(history).last();
        var nextBuildName = buildNames[buildIndex];

        this._getFailingTests(nextBuildName, function(tests, tooManyFailures) {
            history[nextBuildName] = {
                tooManyFailures: tooManyFailures,
                tests: {},
            };

            for (var testName in tests) {
                if (previousBuildName) {
                    if (!(testName in history[previousBuildName].tests))
                        continue;
                    delete history[previousBuildName].tests[testName];
                }
                history[nextBuildName].tests[testName] = tests[testName];
            }

            callback(Object.keys(history[nextBuildName].tests).length);
        },
        function(tests) {
            // Some tests failed, but we couldn't fetch results.html (perhaps because the test
            // run aborted early for some reason). Just skip this build entirely.
            callback(true);
        });
    },
};
