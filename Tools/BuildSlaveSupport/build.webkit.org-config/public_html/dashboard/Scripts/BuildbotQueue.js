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

BuildbotQueue = function(buildbot, id, info)
{
    BaseObject.call(this);

    console.assert(buildbot);
    console.assert(id);

    this.buildbot = buildbot;
    this.id = id;

    this.branch = info.branch || null;
    this.platform = info.platform.name || "unknown";
    this.debug = info.debug || false;
    this.builder = info.builder || false;
    this.tester = info.tester || false;
    this.performance = info.performance || false;
    this.leaks = info.leaks || false;
    this.architecture = info.architecture || null;
    this.testCategory = info.testCategory || null;
    this.performanceTestName = info.performanceTestName || null;

    this.iterations = [];
    this._knownIterations = {};
};

BaseObject.addConstructorFunctions(BuildbotQueue);

BuildbotQueue.RecentIterationsToLoad = 10;

BuildbotQueue.Event = {
    IterationsAdded: "iterations-added",
    UnauthorizedAccess: "unauthorized-access"
};

BuildbotQueue.prototype = {
    constructor: BuildbotQueue,
    __proto__: BaseObject.prototype,

    get baseURL()
    {
        return this.buildbot.baseURL + "json/builders/" + encodeURIComponent(this.id);
    },

    get allIterationsURL()
    {
        // Getting too many builds results in a timeout error, 10000 is OK.
        return this.buildbot.baseURL + "json/builders/" + encodeURIComponent(this.id) + "/builds/_all/?max=10000";
    },

    get overviewURL()
    {
        return this.buildbot.baseURL + "builders/" + encodeURIComponent(this.id) + "?numbuilds=50";
    },

    get recentFailedIterationCount()
    {
        var firstFinishedIteration = this.mostRecentFinishedIteration;
        var mostRecentSuccessfulIteration = this.mostRecentSuccessfulIteration;
        return this.iterations.indexOf(mostRecentSuccessfulIteration) - this.iterations.indexOf(firstFinishedIteration);
    },

    get firstRecentUnsuccessfulIteration()
    {
        if (!this.iterations.length)
            return null;

        for (var i = 0; i < this.iterations.length; ++i) {
            if (!this.iterations[i].finished || !this.iterations[i].successful)
                continue;
            if (this.iterations[i - 1] && this.iterations[i - 1].finished && !this.iterations[i - 1].successful)
                return this.iterations[i - 1];
            return null;
        }

        if (!this.iterations[this.iterations.length - 1].successful)
            return this.iterations[this.iterations.length - 1];

        return null;
    },

    get mostRecentFinishedIteration()
    {
        for (var i = 0; i < this.iterations.length; ++i) {
            if (!this.iterations[i].finished)
                continue;
            return this.iterations[i];
        }

        return null;
    },

    get mostRecentSuccessfulIteration()
    {
        for (var i = 0; i < this.iterations.length; ++i) {
            if (!this.iterations[i].finished || !this.iterations[i].successful)
                continue;
            return this.iterations[i];
        }

        return null;
    },

    _load: function(url, callback)
    {
        if (this.buildbot.needsAuthentication && this.buildbot.authenticationStatus === Buildbot.AuthenticationStatus.InvalidCredentials)
            return;

        JSON.load(
            url,
            function(data) {
                this.buildbot.isAuthenticated = true;
                callback(data);
            }.bind(this),
            function(data) {
                if (data.errorType !== JSON.LoadError || data.errorHTTPCode !== 401)
                    return;
                if (this.buildbot.isAuthenticated) {
                    // FIXME (128006): Safari/WebKit should coalesce authentication requests with the same origin and authentication realm.
                    // In absence of the fix, Safari presents additional authentication dialogs regardless of whether an earlier authentication
                    // dialog was dismissed. As a way to ameliorate the user experience where a person authenticated successfully using an
                    // earlier authentication dialog and cancelled the authentication dialog associated with the load for this queue, we call
                    // ourself so that we can schedule another load, which should complete successfully now that we have credentials.
                    this._load(url, callback);
                    return;
                }

                this.buildbot.isAuthenticated = false;
                this.dispatchEventToListeners(BuildbotQueue.Event.UnauthorizedAccess, { });
            }.bind(this),
            {withCredentials: this.buildbot.needsAuthentication}
        );
    },

    loadMoreHistoricalIterations: function()
    {
        var indexOfFirstNewlyLoadingIteration;
        for (var i = 0; i < this.iterations.length; ++i) {
            if (indexOfFirstNewlyLoadingIteration !== undefined && i >= indexOfFirstNewlyLoadingIteration + BuildbotQueue.RecentIterationsToLoad)
                return;
            var iteration = this.iterations[i];
            if (!iteration.finished)
                continue;
            if (iteration.isLoading) {
                // Caller lacks visibility into loading, so it is likely to call this function too often.
                // Give it a chance to analyze everything that's been already requested first, and then it can decide whether it needs more.
                return;
            }
            if (iteration.loaded && indexOfFirstNewlyLoadingIteration !== undefined) {
                // There was a gap between loaded iterations, which we've closed now.
                return;
            }
            if (!iteration.loaded) {
                if (indexOfFirstNewlyLoadingIteration === undefined)
                    indexOfFirstNewlyLoadingIteration = i;
                iteration.update();
            }
        }
    },

    update: function()
    {
        this._load(this.baseURL, function(data) {
            if (!(data.cachedBuilds instanceof Array))
                return;

            var currentBuilds = {};
            if (data.currentBuilds instanceof Array)
                data.currentBuilds.forEach(function(id) { currentBuilds[id] = true; });

            var loadingStop = Math.max(0, data.cachedBuilds.length - BuildbotQueue.RecentIterationsToLoad);

            var newIterations = [];

            for (var i = data.cachedBuilds.length - 1; i >= 0; --i) {
                var iteration = this._knownIterations[data.cachedBuilds[i]];
                if (!iteration) {
                    iteration = new BuildbotIteration(this, parseInt(data.cachedBuilds[i], 10), !(data.cachedBuilds[i] in currentBuilds));
                    newIterations.push(iteration);
                    this.iterations.push(iteration);
                    this._knownIterations[iteration.id] = iteration;
                }

                if (i >= loadingStop && (!iteration.finished || !iteration.loaded))
                    iteration.update();
            }

            if (!newIterations.length)
                return;

            this.sortIterations();

            this.dispatchEventToListeners(BuildbotQueue.Event.IterationsAdded, {addedIterations: newIterations});
        }.bind(this));
    },

    loadAll: function(callback)
    {
        // FIXME: Don't load everything at once, do it incrementally as requested.
        this._load(this.allIterationsURL, function(data) {
            for (var idString in data) {
                console.assert(typeof idString === "string");
                var iteration = new BuildbotIteration(this, data[idString]);
                this.iterations.push(iteration);
                this._knownIterations[iteration.id] = iteration;
            }

            this.sortIterations();

            callback(this);
        }.bind(this));
    },

    sortIterations: function()
    {
        function compareIterations(a, b)
        {
            var result = b.openSourceRevision - a.openSourceRevision;
            if (result)
                return result;

            result = b.internalRevision - a.internalRevision;
            if (result)
                return result;

            return b.id - a.id;
        }

        this.iterations.sort(compareIterations);
    }
};
