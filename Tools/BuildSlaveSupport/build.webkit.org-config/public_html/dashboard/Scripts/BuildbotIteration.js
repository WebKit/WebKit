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

BuildbotIteration = function(queue, dataOrID, finished)
{
    BaseObject.call(this);

    console.assert(queue);

    this.queue = queue;

    if (typeof dataOrID === "object") {
        this._parseData(dataOrID);
        return;
    }

    console.assert(typeof dataOrID === "number");
    this.id = dataOrID;

    this.loaded = false;
    this.isLoading = false;
    this._productive = false;

    this.revision = {};

    this.layoutTestResults = null; // Layout test results can be needed even if all tests passed, e.g. for the leaks bot.

    this.failedTestSteps = [];

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

// If none of these steps ran, then we didn't get any real results, and the iteration was not productive.
// All test steps are considered productive too.
BuildbotIteration.ProductiveSteps = {
    "Build" : 1,
    "build ASan archive": 1,
    "compile-webkit": 1,
    "scan build": 1,
};

// These have a special meaning for test queue views.
BuildbotIteration.TestSteps = {
    "API tests": "platform api test",
    "bindings-generation-tests": "bindings tests",
    "jscore-test": "javascript test",
    "layout-test": "layout test",
    "perf-test": "performance test",
    "run-api-tests": "api test",
    "webkit-32bit-jsc-test": "javascript test",
    "webkit-jsc-cloop-test": "javascript cloop test",
    "webkitperl-test": "webkitperl test",
    "webkitpy-test": "webkitpy test",
};

BuildbotIteration.Event = {
    Updated: "updated",
    UnauthorizedAccess: "unauthorized-access"
};

// See <http://docs.buildbot.net/0.8.8/manual/cfg-properties.html>.
function isMultiCodebaseGotRevisionProperty(property)
{
    return property[0] === "got_revision" && typeof property[1] === "object";
}

function parseRevisionProperty(property, key, fallbackKey)
{
    if (!property)
        return null;
    var value = property[1];

    // The property got_revision may have the following forms:
    //
    // ["got_revision",{"Internal":"1357","WebKitOpenSource":"2468"},"Source"]
    // OR
    // ["got_revision","2468","Source"]
    if (isMultiCodebaseGotRevisionProperty(property))
        value = (key in value) ? value[key] : value[fallbackKey];
    return value;
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
        return this._productive;
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

    _parseData: function(data)
    {
        console.assert(!this.id || this.id === data.number);
        this.id = data.number;

        this.revision = {};
        var revisionProperty = data.properties.findFirst(function(property) {
            return property[0] === "got_revision";
        });
        var branches = this.queue.branches;
        for (var i = 0; i < branches.length; ++i) {
            var repository = branches[i].repository;
            var repositoryName = repository.name;
            var key;
            var fallbackKey;
            if (repository === Dashboard.Repository.OpenSource) {
                key = "WebKit";
                fallbackKey = "opensource";
            } else if (repository === Dashboard.Repository.Internal) {
                key = "Internal";
                fallbackKey = "internal";
            } else {
                key = repositoryName;
                fallbackKey = null;
            }
            var revision = parseRevisionProperty(revisionProperty, key, fallbackKey);
            this.revision[repositoryName] = revision;
        }

        function sourceStampChanges(sourceStamp) {
            var result = [];
            var changes = sourceStamp.changes;
            for (var i = 0; i < changes.length; ++i) {
                var change = { revisionNumber: changes[i].revision }
                if (changes[i].repository)
                    change.repository = changes[i].repository;
                if (changes[i].branch)
                    change.branch = changes[i].branch;
                // There is also a timestamp, but it's not accurate.
                result.push(change);
            }
            return result;
        }

        // The changes array is generally meaningful for svn triggered queues (such as builders),
        // but not for internally triggered ones (such as testers), due to coalescing.
        this.changes = [];
        if (data.sourceStamp)
            this.changes = sourceStampChanges(data.sourceStamp);
        else for (var i = 0; i < data.sourceStamps.length; ++i)
            this.changes = this.changes.concat(sourceStampChanges(data.sourceStamps[i]));

        this.startTime = new Date(data.times[0] * 1000);
        this.endTime = new Date(data.times[1] * 1000);

        this.failedTestSteps = [];
        data.steps.forEach(function(step) {
            if (!step.isFinished || step.hidden || !(step.name in BuildbotIteration.TestSteps))
                return;
            var results = new BuildbotTestResults(step);
            if (step.name === "layout-test")
                this.layoutTestResults = results;
            if (results.allPassed)
                return;
            this.failedTestSteps.push(results);
        }, this);

        var masterShellCommandStep = data.steps.findFirst(function(step) { return step.name === "MasterShellCommand"; });
        this.resultURLs = masterShellCommandStep ? masterShellCommandStep.urls : null;
        for (var linkName in this.resultURLs) {
            var url = this.resultURLs[linkName];
            if (!url.startsWith("http"))
                this.resultURLs[linkName] = this.queue.buildbot.baseURL + url;
        }

        this.loaded = true;

        this._firstFailedStep = data.steps.findFirst(function(step) { return !step.hidden && step.results[0] === BuildbotIteration.FAILURE; });

        console.assert(data.results === null || typeof data.results === "number");
        this._result = data.results;

        this.text = data.text.join(" ");

        if (!data.currentStep)
            this.finished = true;

        this._productive = this._finished && this._result !== BuildbotIteration.EXCEPTION && this._result !== BuildbotIteration.RETRY;
        if (this._productive) {
            var finishedAnyProductiveStep = false;
            for (var i = 0; i < data.steps.length; ++i) {
                var step = data.steps[i];
                if (!step.isFinished)
                    break;
                if (step.name in BuildbotIteration.ProductiveSteps || step.name in BuildbotIteration.TestSteps) {
                    finishedAnyProductiveStep = true;
                    break;
                }
            }
            this._productive = finishedAnyProductiveStep;
        }
    },

    _updateWithData: function(data)
    {
        if (this.loaded && this._finished)
            return;

        this._parseData(data);

        // Update the sorting since it is based on the revision numbers that just became known.
        this.queue.sortIterations();

        this.dispatchEventToListeners(BuildbotIteration.Event.Updated);
    },

    update: function()
    {
        if (this.loaded && this._finished)
            return;

        if (this.queue.buildbot.needsAuthentication && this.queue.buildbot.authenticationStatus === Buildbot.AuthenticationStatus.InvalidCredentials)
            return;

        if (this.isLoading)
            return;

        this.isLoading = true;

        JSON.load(this.queue.baseURL + "/builds/" + this.id, function(data) {
            this.isLoading = false;
            this.queue.buildbot.isAuthenticated = true;
            if (!data || !data.properties)
                return;

            this._updateWithData(data);
        }.bind(this),
        function(data) {
            this.isLoading = false;
            if (data.errorType === JSON.LoadError && data.errorHTTPCode === 401) {
                this.queue.buildbot.isAuthenticated = false;
                this.dispatchEventToListeners(BuildbotIteration.Event.UnauthorizedAccess);
            }
        }.bind(this), {withCredentials: this.queue.buildbot.needsAuthentication});
    },

    loadLayoutTestResults: function(callback)
    {
        if (this.queue.buildbot.needsAuthentication && this.queue.buildbot.authenticationStatus === Buildbot.AuthenticationStatus.InvalidCredentials)
            return;

        JSON.load(this.queue.buildbot.layoutTestFullResultsURLForIteration(this), function(data) {
            this.queue.buildbot.isAuthenticated = true;

            this.layoutTestResults.addFullLayoutTestResults(data);
            callback();
        }.bind(this),
        function(data) {
            if (data.errorType === JSON.LoadError && data.errorHTTPCode === 401) {
                this.queue.buildbot.isAuthenticated = false;
                this.dispatchEventToListeners(BuildbotIteration.Event.UnauthorizedAccess);
            }
            console.log(data.error);
            callback();
        }.bind(this), {jsonpCallbackName: "ADD_RESULTS", withCredentials: this.queue.buildbot.needsAuthentication});
    }
};
