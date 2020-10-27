/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

Buildbot = function(baseURL, queuesInfo, options)
{
    BaseObject.call(this);

    console.assert(baseURL);
    console.assert(queuesInfo);

    this.baseURL = baseURL;
    this.queuesInfo = queuesInfo;
    this.queues = {};

    this._normalizeQueuesInfo();

    // We regard _needsAuthentication as a hint whether this Buildbot requires authentication so that we can show
    // an appropriate initial status message (say, an "unauthorized" status if the Buildbot requires authentication)
    // for its associated queues before we make the actual HTTP request for the status of each queue.
    this._needsAuthentication = typeof options === "object" && options.needsAuthentication === true;
    this._authenticationStatus = Buildbot.AuthenticationStatus.Unauthenticated;

    this.VERSION_LESS_THAN_09 = options && options.USE_BUILDBOT_VERSION_LESS_THAN_09;

    if (!this.VERSION_LESS_THAN_09) {
        this._builderNameToIDMap = {};
        this._computeBuilderNameToIDMap();
    }

    for (var id in queuesInfo) {
        if (queuesInfo[id].combinedQueues) {
            for (var combinedQueueID in queuesInfo[id].combinedQueues)
                this.queues[combinedQueueID] = new BuildbotQueue(this, combinedQueueID, queuesInfo[id].combinedQueues[combinedQueueID]);
        } else
            this.queues[id] = new BuildbotQueue(this, id, queuesInfo[id]);
    }
};

BaseObject.addConstructorFunctions(Buildbot);

Buildbot.AuthenticationStatus = {
    Unauthenticated: "unauthenticated",
    Authenticated: "authenticated",
    InvalidCredentials: "invalid-credentials"
};

Buildbot.UpdateReason = {
    Reauthenticate: "reauthenticate"
};

// Ordered importance.
Buildbot.TestCategory = {
    WebKit2: "webkit-2",
    WebKit1: "webkit-1"
};

// Ordered importance.
Buildbot.BuildArchitecture = {
    Universal: "universal",
    SixtyFourBit: "sixty-four-bit",
    ThirtyTwoBit: "thirty-two-bit"
};

Buildbot.prototype = {
    constructor: Buildbot,
    __proto__: BaseObject.prototype,

    get needsAuthentication()
    {
        return this._needsAuthentication;
    },

    get authenticationStatus()
    {
        return this._authenticationStatus;
    },

    get isAuthenticated()
    {
        return this._authenticationStatus === Buildbot.AuthenticationStatus.Authenticated;
    },

    set isAuthenticated(value)
    {
        this._authenticationStatus = value ? Buildbot.AuthenticationStatus.Authenticated : Buildbot.AuthenticationStatus.InvalidCredentials;
    },

    _normalizeQueueInfo: function(queueInfo)
    {
        if (!queueInfo.combinedQueues)
            queueInfo.branches = queueInfo.branches || this.defaultBranches;
        queueInfo.debug = queueInfo.debug || false;
        queueInfo.builder = queueInfo.builder || false;
        queueInfo.tester = queueInfo.tester || false;
        queueInfo.performance = queueInfo.performance || false;
        queueInfo.staticAnalyzer = queueInfo.staticAnalyzer || false;
        queueInfo.leaks = queueInfo.leaks || false;
        queueInfo.architecture = queueInfo.architecture || null;
        queueInfo.testCategory = queueInfo.testCategory || null;
        queueInfo.heading = queueInfo.heading || null;
        queueInfo.crashesOnly = queueInfo.crashesOnly || false;
    },

    _normalizeQueuesInfo: function()
    {
        for (queueName in this.queuesInfo) {
            var queueInfo = this.queuesInfo[queueName];
            this._normalizeQueueInfo(queueInfo);
            if (queueInfo.combinedQueues) {
                for (combinedQueueName in queueInfo.combinedQueues) {
                    queueInfo.combinedQueues[combinedQueueName].platform = queueInfo.platform;
                    this._normalizeQueueInfo(queueInfo.combinedQueues[combinedQueueName]);
                }
            }
        }
    },

    updateQueues: function(updateReason)
    {
        var shouldReauthenticate = updateReason === Buildbot.UpdateReason.Reauthenticate;
        if (shouldReauthenticate) {
            var savedAuthenticationStatus = this._authenticationStatus;
            this._authenticationStatus = Buildbot.AuthenticationStatus.Unauthenticated;
        }
        for (var id in this.queues)
            this.queues[id].update();
        if (shouldReauthenticate) {
            // Assert status wasn't changed synchronously. Otherwise, we will override it (below).
            console.assert(this._authenticationStatus === Buildbot.AuthenticationStatus.Unauthenticated);
            this._authenticationStatus = savedAuthenticationStatus;
        }
    },

    // FIXME: Remove this logic after <https://github.com/buildbot/buildbot/issues/3465> is fixed.
    _computeBuilderNameToIDMap: function()
    {
        JSON.load(this.baseURL + "api/v2/builders", function(data) {
            if (!data || !(data.builders instanceof Array))
                return;

            for (var builder of data.builders)
                this._builderNameToIDMap[builder.name] = builder.builderid;
        }.bind(this));
    },

    buildPageURLForIteration: function(iteration)
    {
        if (this.VERSION_LESS_THAN_09)
            return this.baseURL + "builders/" + encodeURIComponent(iteration.queue.id) + "/builds/" + iteration.id;

        // FIXME: Remove this._builderNameToIDMap lookup after <https://github.com/buildbot/buildbot/issues/3465> is fixed.
        return this.baseURL + "#/builders/" + encodeURIComponent(this._builderNameToIDMap[iteration.queue.id]) + "/builds/" + iteration.id;
    },

    javaScriptCoreTestFailuresURLForIteration: function(iteration, name)
    {
        return this.buildPageURLForIteration(iteration) + "/steps/" + name + "/logs/json/text";
    },

    javaScriptCoreTestStdioUrlForIteration: function(iteration, name)
    {
        return this.buildPageURLForIteration(iteration) + "/steps/" + name + "/logs/stdio";
    },

    layoutTestResultsDirectoryURLForIteration: function(iteration)
    {
        var underscoreSeparatedRevisions = "r";
        sortDictionariesByOrder(Dashboard.Repository).forEach(function(repository) {
            if (iteration.revision[repository.name]) {
                if (underscoreSeparatedRevisions.length > 1)
                    underscoreSeparatedRevisions += "_";
                underscoreSeparatedRevisions += iteration.revision[repository.name];
            }
        });
        return this.baseURL + "results/" + encodeURIComponent(iteration.queue.id) + "/" + encodeURIComponent(underscoreSeparatedRevisions + " (" + iteration.id + ")");
    },

    layoutTestResultsURLForIteration: function(iteration)
    {
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/results.html";
    },

    dashboardTestResultsURLForIteration: function(iteration)
    {
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/dashboard-layout-test-results/index-pretty-diff.html";
    },

    layoutTestFullResultsURLForIteration: function(iteration)
    {
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/full_results.json";
    },

    layoutTestCrashLogURLForIteration: function(iteration, testPath)
    {
        var path = testPath.replace(/^(.*)\.(?:.*)$/, "$1-crash-log.txt");
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/" + path;
    },

    layoutTestStderrURLForIteration: function(iteration, testPath)
    {
        var path = testPath.replace(/^(.*)\.(?:.*)$/, "$1-stderr.txt");
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/" + path;
    },

    layoutTestDiffURLForIteration: function(iteration, testPath)
    {
        var path = testPath.replace(/^(.*)\.(?:.*)$/, "$1-diff.txt");
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/" + path;
    },

    layoutTestPrettyDiffURLForIteration: function(iteration, testPath)
    {
        // pretty-patch may not be available, caller should check JSON results for has_pretty_patch attribute.
        var path = testPath.replace(/^(.*)\.(?:.*)$/, "$1-pretty-diff.html");
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/" + path;
    },

    layoutTestImagesURLForIteration: function(iteration, testPath)
    {
        var path = testPath.replace(/^(.*)\.(?:.*)$/, "$1-diffs.html");
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/" + path;
    },

    layoutTestImageDiffURLForIteration: function(iteration, testPath)
    {
        var path = testPath.replace(/^(.*)\.(?:.*)$/, "$1-diff.png");
        return this.layoutTestResultsDirectoryURLForIteration(iteration) + "/" + path;
    },
};
