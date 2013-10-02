/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
    console.assert(id);

    this.queue = queue;
    this.id = id;

    this.loaded = false;

    this.openSourceRevision = null;
    this.internalRevision = null;

    this.layoutTestResults = null;
    this.javascriptTestResults = null;
    this.pythonTestResults = null;
    this.perlTestResults = null;
    this.bindingTestResults = null;

    this._finished = finished;
};

BaseObject.addConstructorFunctions(BuildbotIteration);

BuildbotIteration.Event = {
    Updated: "updated"
};

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

    update: function()
    {
        if (this.loaded && this._finished)
            return;

        function collectTestResults(data, stepName)
        {
            var testStep = data.steps.findFirst(function(step) { return step.name === stepName; });
            if (!testStep)
                return null;

            if (testStep.results[0] === 4) {
                // This build step was interrupted (perhaps due to the build slave restarting).
                return null;
            }

            var testResults = {};

            if (!("isFinished" in testStep)) {
                // The step never even ran, or hasn't finish running.
                testResults.finished = false;
                return testResults;
            }

            testResults.finished = true;

            if (!("results" in testStep) || !testStep.results[0]) {
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
            this.openSourceRevision = openSourceRevisionProperty ? parseInt(openSourceRevisionProperty[1], 10) : null;

            var internalRevisionProperty = data.properties.findFirst(function(property) { return property[0] === "internal_got_revision"; });
            this.internalRevision = internalRevisionProperty ? parseInt(internalRevisionProperty[1], 10) : null;

            if (!this.layoutTestResults || !this.layoutTestResults.finished) {
                var layoutTestResults = collectTestResults.call(this, data, "layout-test");
                this.layoutTestResults = layoutTestResults ? new BuildbotTestResults(this, layoutTestResults) : null;
            }

            if (!this.javascriptTestResults || !this.javascriptTestResults.finished) {
                var javascriptTestResults = collectTestResults.call(this, data, "jscore-test");
                this.javascriptTestResults = javascriptTestResults ? new BuildbotTestResults(this, javascriptTestResults) : null;
            }

            if (!this.apiTestResults || !this.apiTestResults.finished) {
                var apiTestResults = collectTestResults.call(this, data, "run-api-tests");
                this.apiTestResults = apiTestResults ? new BuildbotTestResults(this, apiTestResults) : null;
            }

            if (!this.pythonTestResults || !this.pythonTestResults.finished) {
                var pythonTestResults = collectTestResults.call(this, data, "webkitpy-test");
                this.pythonTestResults = pythonTestResults ? new BuildbotTestResults(this, pythonTestResults) : null;
            }

            if (!this.perlTestResults || !this.perlTestResults.finished) {
                var perlTestResults = collectTestResults.call(this, data, "webkitperl-test");
                this.perlTestResults = perlTestResults ? new BuildbotTestResults(this, perlTestResults) : null;
            }

            if (!this.bindingTestResults || !this.bindingTestResults.finished) {
                var bindingTestResults = collectTestResults.call(this, data, "bindings-generation-tests");
                this.bindingTestResults = bindingTestResults ? new BuildbotTestResults(this, bindingTestResults) : null;
            }

            this.loaded = true;

            this.failed = !!data.results;

            if (!data.currentStep)
                this.finished = true;

            // Update the sorting since it is based on the revisions we just loaded.
            this.queue.sortIterations();

            this.dispatchEventToListeners(BuildbotIteration.Event.Updated);
        }.bind(this));
    }
};
