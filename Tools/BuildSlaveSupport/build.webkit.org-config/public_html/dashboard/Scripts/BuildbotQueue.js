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
    this.architecture = info.architecture || null;
    this.testCategory = info.testCategory || null;

    this.iterations = [];
    this._knownIterations = {};

    this.update();
};

BaseObject.addConstructorFunctions(BuildbotQueue);

BuildbotQueue.MaximumQueuesToLoad = 10;

BuildbotQueue.Event = {
    IterationsAdded: "iterations-added"
};

BuildbotQueue.prototype = {
    constructor: BuildbotQueue,
    __proto__: BaseObject.prototype,

    get baseURL()
    {
        return this.buildbot.baseURL + "json/builders/" + encodeURIComponent(this.id);
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

    update: function(iterationsToLoad)
    {
        JSON.load(this.baseURL, function(data) {
            if (!(data.cachedBuilds instanceof Array))
                return;

            var currentBuilds = {};
            if (data.currentBuilds instanceof Array)
                data.currentBuilds.forEach(function(id) { currentBuilds[id] = true; });

            if (!iterationsToLoad)
                iterationsToLoad = data.cachedBuilds.length;

            var stop = Math.max(0, data.cachedBuilds.length - iterationsToLoad);
            var loadingStop = Math.max(0, data.cachedBuilds.length - BuildbotQueue.MaximumQueuesToLoad);

            var newIterations = [];

            for (var i = data.cachedBuilds.length - 1; i >= stop; --i) {
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
        }.bind(this), {withCredentials: this.buildbot.needsAuthentication});
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
