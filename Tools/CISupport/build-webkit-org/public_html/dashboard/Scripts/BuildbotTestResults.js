/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

BuildbotTestResults = function(testStep, url)
{
    BaseObject.call(this);
    this.URL = url;

    this._parseResults(testStep);
};

BaseObject.addConstructorFunctions(BuildbotTestResults);

BuildbotTestResults.prototype = {
    constructor: BuildbotTestResults,
    __proto__: BaseObject.prototype,

    _parseResults: function(testStep)
    {
        this.name = testStep.name;
        this.allPassed = false;
        this.errorOccurred = false;
        this.tooManyFailures = false;

        this.failureCount = 0;
        this.flakeyCount = 0;
        this.totalLeakCount = 0;
        this.uniqueLeakCount = 0;
        this.newPassesCount = 0;
        this.missingCount = 0;
        this.crashCount = 0;

        if (!testStep.complete) {
            // The step never even ran, or hasn't finished running.
            this.finished = false;
            return;
        }

        this.finished = true;

        if (!testStep.results || testStep.results === BuildbotIteration.SUCCESS || testStep.results === BuildbotIteration.WARNINGS) {
            // All tests passed.
            this.allPassed = true;
            return;
        }

        if (/Exiting early/.test(testStep.state_string))
            this.tooManyFailures = true;

        function resultSummarizer(matchString, sum, outputLine)
        {
            // Sample outputLine: "53 failures 37 new passes 1 crashes"
            var regex = new RegExp("(\\d+)\\s" + matchString);
            match = regex.exec(outputLine);
            if (!match) {
                match = /^(\d+)\s/.exec(outputLine);
            }
            if (!match)
                return sum;
            if (!outputLine.contains(matchString))
                return sum;
            if (!sum || sum === -1)
                sum = 0;
            return sum + parseInt(match[1], 10);
        }

        this.failureCount = resultSummarizer('fail', null, testStep.state_string);
        this.flakeyCount = resultSummarizer("flake", null, testStep.state_string);
        this.totalLeakCount = resultSummarizer("total leak", null, testStep.state_string);
        this.uniqueLeakCount = resultSummarizer("unique leak", null, testStep.state_string);
        this.newPassesCount = resultSummarizer("new pass", null, testStep.state_string);
        this.missingCount = resultSummarizer("missing", null, testStep.state_string);
        this.crashCount = resultSummarizer("crash", null, testStep.state_string);
        this.issueCount = resultSummarizer("issue", null, testStep.state_string);

        if (!this.failureCount && !this.flakyCount && !this.totalLeakCount && !this.uniqueLeakCount && !this.newPassesCount && !this.missingCount) {
            // This step exited with a non-zero exit status, but we didn't find any output about the number of failed tests.
            // Something must have gone wrong (e.g., timed out and was killed by buildbot).
            this.errorOccurred = true;
        }
    },

    addFullLayoutTestResults: function(data)
    {
        console.assert(this.name === "layout-test");

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
                        // on retry (except for reftests), so many times, you will see images on buildbot page, but not on the dashboard.
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

        this.hasPrettyPatch = data.has_pretty_patch;

        this.regressions = collectResults(data.tests, function(info) { return info["report"] === "REGRESSION" });
        console.assert(data.num_regressions === this.regressions.length);

        this.flakyTests = collectResults(data.tests, function(info) { return info["report"] === "FLAKY" });
        console.assert(data.num_flaky === this.flakyTests.length);

        this.testsWithMissingResults = collectResults(data.tests, function(info) { return info["report"] === "MISSING" });
        // data.num_missing is not always equal to the size of testsWithMissingResults array,
        // because buildbot counts regressions that had missing pixel results on retry (e.g. "TEXT MISSING").
        console.assert(data.num_missing >= this.testsWithMissingResults.length);
    },

    addJavaScriptCoreTestFailures: function(data)
    {
        this.regressions = data.stressTestFailures;
    },
};
