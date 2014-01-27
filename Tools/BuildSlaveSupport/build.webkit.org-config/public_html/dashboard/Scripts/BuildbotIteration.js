/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

BuildbotIteration = function(queue, id, finished)
{
    BaseObject.call(this);

    console.assert(queue);

    this.queue = queue;
    this.id = id;

    this.loaded = false;

    this.openSourceRevision = null;
    this.internalRevision = null;

    this.layoutTestResults = null;
    this.javascriptTestResults = null;
    this.apiTestResults = null;
    this.platformAPITestResults = null;
    this.pythonTestResults = null;
    this.perlTestResults = null;
    this.bindingTestResults = null;

    this._finished = finished;
};

BaseObject.addConstructorFunctions(BuildbotIteration);

// JSON result values for both individual steps and the whole iteration.
BuildbotIteration.SUCCESS = 0;
BuildbotIteration.WARNINGS = 1;
BuildbotIteration.FAILURE = 2;
BuildbotIteration.SKIPPED = 3;
BuildbotIteration.EXCEPTION = 4;
BuildbotIteration.RETRY = 5;

BuildbotIteration.Event = {
    Updated: "updated"
};

function parseRevisionProperty(property, key)
{
    if (!property)
        return null;

    // The property "got_revision" is a dictionary for multi-codebase builds
    // according to <http://docs.buildbot.net/0.8.8/manual/cfg-properties.html>.
    var value = property[1];
    return parseInt(typeof value == "string" ? value : value[key], 10);
}

BuildbotIteration.prototype = {
    constructor: BuildbotIteration,
    __proto__: BaseObject.prototype,

    get finished()
    {
        return this._finished;
    },

    set finished(x)
    {
        this._finished = x;
    },

    get successful()
    {
        return this._result === BuildbotIteration.SUCCESS;
    },

    get productive()
    {
        return this.loaded && this._finished && this._result !== BuildbotIteration.EXCEPTION && this._result !== BuildbotIteration.RETRY;
    },

    // It is not a real failure if Buildbot itself failed with codes like EXCEPTION or RETRY.
    get failed()
    {
        return this._result === BuildbotIteration.FAILURE;
    },

    get firstFailedStepName()
    {
        if (!this._firstFailedStep)
            return undefined;
        return this._firstFailedStep.name;
    },

    failureLogURL: function(kind)
    {
        if (!this.failed)
            return undefined;

        console.assert(this._firstFailedStep);

        for (var i = 0; i < this._firstFailedStep.logs.length; ++i) {
            if (this._firstFailedStep.logs[i][0] == kind)
                return this._firstFailedStep.logs[i][1];
        }

        return undefined;
    },

    get failureLogs()
    {
        if (!this.failed)
            return undefined;

        console.assert(this._firstFailedStep);
        return this._firstFailedStep.logs;
    },

    get previousProductiveIteration()
    {
        for (var i = 0; i < this.queue.iterations.length - 1; ++i) {
            if (this.queue.iterations[i] === this) {
                while (++i < this.queue.iterations.length) {
                    var iteration = this.queue.iterations[i];
                    if (iteration.productive)
                        return iteration;
                }
                break;
            }
        }
        return null;
    },

    update: function()
    {
        if (this.loaded && this._finished)
            return;

        function collectTestResults(data, stepName)
        {
            var testStep = data.steps.findFirst(function(step) { return step.name === stepName; });
            if (!testStep)
                return null;

            var testResults = {};

            if (!testStep.isFinished) {
                // The step never even ran, or hasn't finish running.
                testResults.finished = false;
                return testResults;
            }

            testResults.finished = true;

            if (!testStep.results || !testStep.results[0]) {
                // All tests passed.
                testResults.allPassed = true;
                return testResults;
            }

            if (/Exiting early/.test(testStep.results[1][0]))
                testResults.tooManyFailures = true;

            function resultSummarizer(matchString, sum, outputLine)
            {
                var match = /^(\d+)\s/.exec(outputLine);
                if (!match)
                    return sum;
                if (!outputLine.contains(matchString))
                    return sum;
                if (!sum || sum === -1)
                    sum = 0;
                return sum + parseInt(match[1], 10);
            }

            testResults.failureCount = testStep.results[1].reduce(resultSummarizer.bind(null, "fail"), undefined);
            testResults.flakeyCount = testStep.results[1].reduce(resultSummarizer.bind(null, "flake"), undefined);
            testResults.totalLeakCount = testStep.results[1].reduce(resultSummarizer.bind(null, "total leak"), undefined);
            testResults.uniqueLeakCount = testStep.results[1].reduce(resultSummarizer.bind(null, "unique leak"), undefined);
            testResults.newPassesCount = testStep.results[1].reduce(resultSummarizer.bind(null, "new pass"), undefined);
            testResults.missingCount = testStep.results[1].reduce(resultSummarizer.bind(null, "missing"), undefined);

            if (!testResults.failureCount && !testResults.flakyCount && !testResults.totalLeakCount && !testResults.uniqueLeakCount && !testResults.newPassesCount && !testResults.missingCount) {
                // This step exited with a non-zero exit status, but we didn't find any output about the number of failed tests.
                // Something must have gone wrong (e.g., timed out and was killed by buildbot).
                testResults.errorOccurred = true;
            }

            return testResults;
        }

        JSON.load(this.queue.baseURL + "/builds/" + this.id, function(data) {
            if (!data || !data.properties)
                return;

            var openSourceRevisionProperty = data.properties.findFirst(function(property) { return property[0] === "got_revision" || property[0] === "revision" || property[0] === "opensource_got_revision"; });
            this.openSourceRevision = parseRevisionProperty(openSourceRevisionProperty, "WebKitOpenSource");

            var internalRevisionProperty = data.properties.findFirst(function(property) { return property[0] === "internal_got_revision" || property[0] === "got_revision"; });
            this.internalRevision = parseRevisionProperty(internalRevisionProperty, "Internal");

            var layoutTestResults = collectTestResults.call(this, data, "layout-test");
            this.layoutTestResults = layoutTestResults ? new BuildbotTestResults(this, layoutTestResults) : null;

            var javascriptTestResults = collectTestResults.call(this, data, "jscore-test");
            this.javascriptTestResults = javascriptTestResults ? new BuildbotTestResults(this, javascriptTestResults) : null;

            var apiTestResults = collectTestResults.call(this, data, "run-api-tests");
            this.apiTestResults = apiTestResults ? new BuildbotTestResults(this, apiTestResults) : null;

            var platformAPITestResults = collectTestResults.call(this, data, "API tests");
            this.platformAPITestResults = platformAPITestResults ? new BuildbotTestResults(this, platformAPITestResults) : null;

            var pythonTestResults = collectTestResults.call(this, data, "webkitpy-test");
            this.pythonTestResults = pythonTestResults ? new BuildbotTestResults(this, pythonTestResults) : null;

            var perlTestResults = collectTestResults.call(this, data, "webkitperl-test");
            this.perlTestResults = perlTestResults ? new BuildbotTestResults(this, perlTestResults) : null;

            var bindingTestResults = collectTestResults.call(this, data, "bindings-generation-tests");
            this.bindingTestResults = bindingTestResults ? new BuildbotTestResults(this, bindingTestResults) : null;

            this.loaded = true;

            this._firstFailedStep = data.steps.findFirst(function(step) { return step.results[0] === BuildbotIteration.FAILURE; });

            console.assert(data.results === null || typeof data.results === "number");
            this._result = data.results;

            this.text = data.text.join(" ");

            if (!data.currentStep)
                this.finished = true;

            // Update the sorting since it is based on the revisions we just loaded.
            this.queue.sortIterations();

            this.dispatchEventToListeners(BuildbotIteration.Event.Updated);
        }.bind(this));
    },

    loadLayoutTestResults: function(callback)
    {
        function collectResults(subtree, predicate)
        {
            // Results object is a trie:
            // directory
            //   subdirectory
            //     test1.html
            //       expected:"PASS"
            //       actual: "IMAGE"
            //       report: "REGRESSION"
            //     test2.html
            //       expected:"FAIL"
            //       actual:"TEXT"

            var result = [];
            for (var key in subtree) {
                var value = subtree[key];
                console.assert(typeof value === "object");
                var isIndividualTest = value.hasOwnProperty("actual") && value.hasOwnProperty("expected");
                if (isIndividualTest) {
                    // Possible values for actual and expected keys: PASS, FAIL, AUDIO, IMAGE, TEXT, IMAGE+TEXT, TIMEOUT, CRASH, MISSING.
                    // Both actual and expected can be space separated lists. Actual contains two values when retrying a failed test
                    // gives a different result (retrying may be disabled in tester configuration).
                    // Possible values for report key (when present): REGRESSION, MISSING, FLAKY.

                    if (predicate(value)) {
                        var item = {path: key};

                        // FIXME (bug 127186): Crash log URL will be incorrect if crash only happened on retry (e.g. "TEXT CRASH").
                        // It should point to retries subdirectory, but the information about which attempt failed gets lost here.
                        if (value.actual.contains("CRASH"))
                            item.crash = true;
                        if (value.actual.contains("TIMEOUT"))
                            item.timeout = true;

                        // FIXME (bug 127186): Similarly, we don't have a good way to present results for something like "TIMEOUT TEXT",
                        // not even UI wise. For now, only show a diff link if the first attempt has the diff.
                        if (value.actual.split(" ")[0].contains("TEXT"))
                            item.has_diff = true;

                        // FIXME (bug 127186): It is particularly unfortunate for image diffs, because we currently only check image results
                        // on retry (except for reftests), so many times, you will see images on buidbot page, but not on the dashboard.
                        // FIXME: Find a way to display expected mismatch reftest failures. 
                        if (value.actual.split(" ")[0].contains("IMAGE") && value.reftest_type != "!=")
                            item.has_image_diff = true;

                        if (value.has_stderr)
                            item.has_stderr = true;

                        result.push(item);
                    }

                } else {
                    var nestedTests = collectResults(value, predicate);
                    for (var i = 0, end = nestedTests.length; i < end; ++i)
                        nestedTests[i].path = key + "/" + nestedTests[i].path;
                    result = result.concat(nestedTests);
                }
            }

            return result;
        }

        JSON.load(this.queue.buildbot.layoutTestFullResultsURLForIteration(this), function(data) {
            if (data.error) {
                console.log(data.error);
                callback();
                return;
            }

            this.hasPrettyPatch = data.has_pretty_patch;

            this.layoutTestResults.regressions = collectResults(data.tests, function(info) { return info["report"] === "REGRESSION" });
            console.assert(data.num_regressions === this.layoutTestResults.regressions.length);

            this.layoutTestResults.flakyTests = collectResults(data.tests, function(info) { return info["report"] === "FLAKY" });
            console.assert(data.num_flaky === this.layoutTestResults.flakyTests.length);

            this.layoutTestResults.testsWithMissingResults = collectResults(data.tests, function(info) { return info["report"] === "MISSING" });
            console.assert(data.num_missing === this.layoutTestResults.testsWithMissingResults.length);

            callback();
        }.bind(this), "ADD_RESULTS");
    }
};
