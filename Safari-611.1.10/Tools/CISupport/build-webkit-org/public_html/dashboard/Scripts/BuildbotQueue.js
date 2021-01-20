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

    // FIXME: Some of these are presentation only, and should be handled above BuildbotQueue level.
    this.branches = info.branches;
    this.platform = info.platform.name;
    this.debug = info.debug;
    this.builder = info.builder;
    this.tester = info.tester;
    this.performance = info.performance;
    this.staticAnalyzer = info.staticAnalyzer;
    this.leaks = info.leaks;
    this.architecture = info.architecture;
    this.testCategory = info.testCategory;
    this.heading = info.heading;
    this.crashesOnly = info.crashesOnly;

    this.iterations = [];
    this._knownIterations = {};

    // Some queues process changes out of order, but we need to display results for the latest commit,
    // not the latest build. BuildbotQueue ensures that at least one productive iteration
    // that was run in order gets loaded (if the queue had any productive iterations, of course).
    this._hasLoadedIterationForInOrderResult = false;
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
        if (this.buildbot.VERSION_LESS_THAN_09)
            return this.buildbot.baseURL + "json/builders/" + encodeURIComponent(this.id);

        return this.buildbot.baseURL + "api/v2/builders/" + encodeURIComponent(this.id);
    },

    get allIterationsURL()
    {
        if (this.buildbot.VERSION_LESS_THAN_09)
            return this.baseURL + "/builds/_all/?max=10000";

        return this.baseURL + "/builds?order=-number";
    },

    get buildsURL()
    {
        // We need to limit the number of builds for which we fetch the info, as it would
        // impact performance. For each build, we will be subsequently making REST API calls
        // to fetch detailed build info.
        return this.baseURL + "/builds?order=-number&limit=20";
    },

    get overviewURL()
    {
        if (this.buildbot.VERSION_LESS_THAN_09)
            return this.buildbot.baseURL + "builders/" + encodeURIComponent(this.id) + "?numbuilds=50";

        return this.buildbot.baseURL + "#/builders/" + encodeURIComponent(this.id) + "?numbuilds=50";
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
                if (!this._hasLoadedIterationForInOrderResult)
                    iteration.addEventListener(BuildbotIteration.Event.Updated, this._checkForInOrderResult.bind(this));
                iteration.update();
            }
        }
    },

    get buildsInfoURL()
    {
        if (this.buildbot.VERSION_LESS_THAN_09)
            return this.baseURL;

        return this.buildsURL;
    },

    getBuilds: function(data)
    {
        if (this.buildbot.VERSION_LESS_THAN_09)
            return data.cachedBuilds.reverse();

        return data.builds;
    },

    isBuildComplete: function(data, index)
    {
        if (this.buildbot.VERSION_LESS_THAN_09) {
            var currentBuilds = {};
            if (data.currentBuilds instanceof Array)
                data.currentBuilds.forEach(function(id) { currentBuilds[id] = true; });

            return (!(data.cachedBuilds[index] in currentBuilds));
        }

        return data.builds[index].complete;
    },

    getIterationID: function(data, index)
    {
        if (this.buildbot.VERSION_LESS_THAN_09)
            return data.cachedBuilds[index];

        return data.builds[index].number;
    },

    update: function()
    {
        this._load(this.buildsInfoURL, function(data) {
            var builds = this.getBuilds(data);
            if (!(builds instanceof Array))
                return;

            var newIterations = [];

            for (var i = 0; i < builds.length; ++i) {
                var iterationID = this.getIterationID(data, i);
                var iteration = this._knownIterations[iterationID];
                if (!iteration) {
                    iteration = new BuildbotIteration(this, iterationID, this.isBuildComplete(data, i));
                    newIterations.push(iteration);
                    this.iterations.push(iteration);
                    this._knownIterations[iteration.id] = iteration;
                }

                if (i < BuildbotQueue.RecentIterationsToLoad && (!iteration.finished || !iteration.loaded)) {
                    if (!this._hasLoadedIterationForInOrderResult)
                        iteration.addEventListener(BuildbotIteration.Event.Updated, this._checkForInOrderResult.bind(this));
                    iteration.update();
                }
            }

            if (!newIterations.length)
                return;

            this.sortIterations();

            this.dispatchEventToListeners(BuildbotQueue.Event.IterationsAdded, {addedIterations: newIterations});
        }.bind(this));
    },

    _checkForInOrderResult: function(event)
    {
        if (this._hasLoadedIterationForInOrderResult)
            return;
        var iterationsInOriginalOrder = this.iterations.concat().sort(function(a, b) { return b.id - a.id; });
        for (var i = 0; i < iterationsInOriginalOrder.length - 1; ++i) {
            var i1 = iterationsInOriginalOrder[i];
            var i2 = iterationsInOriginalOrder[i + 1];
            if (i1.productive && i2.loaded && this.compareIterationsByRevisions(i1, i2) < 0) {
                this._hasLoadedIterationForInOrderResult = true;
                return;
            }
        }
        this.loadMoreHistoricalIterations();
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

            this._hasLoadedIterationForInOrderResult = true;

            callback(this);
        }.bind(this));
    },

    compareIterations: function(a, b)
    {
        result = this.compareIterationsByRevisions(a, b);
        if (result)
            return result;

        // A loaded iteration may not have revision numbers if it failed early, before svn steps finished.
        result = b.loaded - a.loaded;
        if (result)
            return result;

        return b.id - a.id;
    },

    compareIterationsByRevisions: function(a, b)
    {
        var sortedRepositories = Dashboard.sortedRepositories;
        for (var i = 0; i < sortedRepositories.length; ++i) {
            var repositoryName = sortedRepositories[i].name;
            var trac = sortedRepositories[i].trac;
            console.assert(trac);
            var indexA = trac.indexOfRevision(a.revision[repositoryName]);
            var indexB = trac.indexOfRevision(b.revision[repositoryName]);
            if (indexA !== -1 && indexB !== -1) {
                var result = indexB - indexA;
                if (result)
                    return result;
            }
        }

        return 0;
    },

    // Re-insert the iteration if its sort order changed (which happens once details about it get loaded).
    updateIterationPosition: function(iteration)
    {
        var oldIndex;
        var inserted = false;
        for (var i = 0; i < this.iterations.length; ++i) {
            if (!inserted && this.compareIterations(this.iterations[i], iteration) > 0) {
                this.iterations.splice(i, 0, iteration);
                if (oldIndex !== undefined)
                    break;
                inserted = true;
                continue;
            }
            if (this.iterations[i] === iteration) {
                oldIndex = i;
                if (inserted)
                    break;
            }
        }
        this.iterations.splice(oldIndex, 1);
    },

    sortIterations: function()
    {
        this.iterations.sort(this.compareIterations.bind(this));
    }
};
