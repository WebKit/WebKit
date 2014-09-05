/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

Analyzer = function()
{
    BaseObject.call(this);

    // Only load one queue at a time to not overload the server.
    this._queueBeingLoaded = null;

    webkitTrac.addEventListener(Trac.Event.Loaded, this._loadedFromTrac, this);
};

BaseObject.addConstructorFunctions(Analyzer);

Analyzer.Event = {
    Starting: "starting",
    QueueResults: "queue-results",
    Finished: "finished",
};

Analyzer.prototype = {
    constructor: Analyzer,
    __proto__: BaseObject.prototype,

    analyze: function(queues, fromDate, toDate)
    {
        this.dispatchEventToListeners(Analyzer.Event.Starting, null);

        this._rangeStartTime = new Date(fromDate);
        this._rangeEndTime = new Date(toDate);

        this._hasTracData = false;

        this._queues = queues;
        this._remainingQueues = {};
        this._queuesReadyToAnalyze = [];

        queues.forEach(function(queue) {
            if (!queue.iterations.length) {
                this._remainingQueues[queue.id] = queue;
                if (!this._queueBeingLoaded) {
                    this._queueBeingLoaded = queue.id;
                    queue.loadAll(this._loadedFromBuildbot.bind(this));
                }
            } else
                this._queuesReadyToAnalyze.push(queue);
        }.bind(this));

        webkitTrac.load(this._rangeStartTime, this._rangeEndTime);
    },

    // Iterations is an array of finished iterations ordered by time, iterations[0] being the newest.
    // Returns an index of an iteration that defined the queue color at the start, or -1
    // if there was none.
    _findIndexOfLargestIterationAtOrBeforeStart: function(iterations)
    {
        var result = -1;
        var i = 0;
        while (i < iterations.length && iterations[i].endTime > this._rangeStartTime)
            ++i;
        if (i < iterations.length)
            result = i++;
        while (i < iterations.length) {
            if (iterations[i].openSourceRevision > iterations[result].openSourceRevision)
                result = i;
            ++i;
        }
        return result;
    },

    _dashboardIsAllGreen: function(topIterationByQueue)
    {
        var countOfQueuesWithKnownColor = Object.keys(topIterationByQueue).length;
        if (countOfQueuesWithKnownColor === 0)
            return undefined;

        for (var queueID in topIterationByQueue) {
            if (!topIterationByQueue[queueID].successful)
                return false;
        }
        return true;
    },

    _updateStretchOfRedCounters: function(topIterationByQueue, currentTime, stretchOfRedCounters)
    {
        var dashboardIsAllGreen = this._dashboardIsAllGreen(topIterationByQueue);

        if (dashboardIsAllGreen === undefined) {
            console.assert(stretchOfRedCounters.currentStretchOfRedStart === undefined);
            console.assert(stretchOfRedCounters.longestStretchOfRed === 0);
            return;
        }

        if (dashboardIsAllGreen) {
            if (stretchOfRedCounters.currentStretchOfRedStart !== undefined) {
                stretchOfRedCounters.longestStretchOfRed = Math.max(stretchOfRedCounters.longestStretchOfRed, currentTime - stretchOfRedCounters.currentStretchOfRedStart);
                stretchOfRedCounters.currentStretchOfRedStart = undefined;
            }
        } else {
            if (stretchOfRedCounters.currentStretchOfRedStart === undefined)
                stretchOfRedCounters.currentStretchOfRedStart = currentTime;
        }
    },

    _countPercentageOfGreen: function(queues, result)
    {
        var topIterationByQueue = {};
        var iterations = [];

        // Find what the top iteration for each queue was at the start of the time range (and thus what colors the dashboard was showing).
        // Merge all queues' iterations into one array, filtering out ones that can't possibly affect the end result.
        queues.forEach(function(queue) {
            var queueIterations = queue.iterations.filter(function(iteration) { return iteration.finished; });
            queueIterations.sort(function(a, b) { return b.endTime - a.endTime; });

            var indexOfLargestIterationAtOrBeforeStart = this._findIndexOfLargestIterationAtOrBeforeStart(queueIterations);
            if (indexOfLargestIterationAtOrBeforeStart >= 0) {
                topIterationByQueue[queue.id] = queueIterations[indexOfLargestIterationAtOrBeforeStart];
                var i = indexOfLargestIterationAtOrBeforeStart;
            } else
                var i = queueIterations.length - 1;
            while (i >= 0 && queueIterations[i].endTime <= this._rangeEndTime)
                iterations.push(queueIterations[i--]);
        }.bind(this));

        iterations.sort(function(a, b) { return b.endTime - a.endTime; });

        // Go forward in time, ignoring out of order iterations that didn't affect queue color.
        var currentTime = this._rangeStartTime;
        var greenTime = 0;
        var stretchOfRedCounters = {
            longestStretchOfRed: 0,
            currentStretchOfRedStart: undefined
        };
        var earliestTimeInRangeWithColor;
        if (this._dashboardIsAllGreen(topIterationByQueue) !== undefined)
            earliestTimeInRangeWithColor = this._rangeStartTime;
        this._updateStretchOfRedCounters(topIterationByQueue, this._rangeStartTime, stretchOfRedCounters);
        for (var i = iterations.length - 1; i >= 0; --i) {
            if (iterations[i].endTime <= this._rangeStartTime) {
                console.assert(iterations[i].queue.id in topIterationByQueue);
                continue;
            }
            if (!(iterations[i].queue.id in topIterationByQueue)) {
                topIterationByQueue[iterations[i].queue.id] = iterations[i];
                if (earliestTimeInRangeWithColor === undefined) {
                    currentTime = iterations[i].endTime;
                    earliestTimeInRangeWithColor = currentTime;
                }
                this._updateStretchOfRedCounters(topIterationByQueue, currentTime, stretchOfRedCounters);
                continue;
            }
            if (iterations[i].openSourceRevision <= topIterationByQueue[iterations[i].queue.id].openSourceRevision)
                continue;

            var dashboardWasAllGreen = this._dashboardIsAllGreen(topIterationByQueue);
            console.assert(dashboardWasAllGreen !== undefined);
            topIterationByQueue[iterations[i].queue.id] = iterations[i];

            if (dashboardWasAllGreen === true)
                greenTime += iterations[i].endTime - currentTime;

            currentTime = iterations[i].endTime;
            this._updateStretchOfRedCounters(topIterationByQueue, currentTime, stretchOfRedCounters);
        }

        if (this._dashboardIsAllGreen(topIterationByQueue) === true)
            greenTime += this._rangeEndTime - currentTime;

        if (stretchOfRedCounters.currentStretchOfRedStart !== undefined)
            stretchOfRedCounters.longestStretchOfRed = Math.max(stretchOfRedCounters.longestStretchOfRed, this._rangeEndTime - stretchOfRedCounters.currentStretchOfRedStart);

        result.longestStretchOfRed = stretchOfRedCounters.longestStretchOfRed / 1000;

        if (earliestTimeInRangeWithColor === this._rangeStartTime)
            result.percentageOfGreen = greenTime / (this._rangeEndTime - this._rangeStartTime) * 100;
        else {
            result.buildbotRangeError = true;
            if (earliestTimeInRangeWithColor !== undefined) {
                result.buildbotRangeErrorText = "Queue only has results starting " + earliestTimeInRangeWithColor;
                result.percentageOfGreen = greenTime / (this._rangeEndTime - earliestTimeInRangeWithColor) * 100;
            } else
                result.buildbotRangeErrorText = "Queue has no results in this range";
        }
    },

    _fullyTestedRevisionNumber: function(lastTestedRevisionByQueue)
    {
        var result = Infinity;
        this._queues.forEach(function(queue) {
            if (lastTestedRevisionByQueue[queue.id] < result)
                result = lastTestedRevisionByQueue[queue.id];
        });
        return result;
    },

    _countTimes: function(queues, result)
    {
        // Combine all iterations that started and finished within the range into one array.
        // These are the iterations that can have results for revisions in the range.
        var iterationsByRevision = [];
        queues.forEach(function(queue) {
            iterationsByRevision = iterationsByRevision.concat(queue.iterations.filter(function(iteration) {
                return iteration.productive && iteration.startTime > this._rangeStartTime && iteration.endTime < this._rangeEndTime;
            }.bind(this)));
        }.bind(this));
        iterationsByRevision.sort(function(a, b) { return a.endTime - b.endTime; }); // When there are multiple iterations building the same revision, the first one wins (as the other ones were probably manual retries).
        iterationsByRevision.sort(function(a, b) { return a.openSourceRevision - b.openSourceRevision; });

        // Revisions that landed within the time range.
        var revisions = webkitTrac.recordedCommits.reduce(function(result, revision) {
            if (revision.date >= this._rangeStartTime && revision.date < this._rangeEndTime)
                result[revision.revisionNumber] = revision;
            return result;
        }.bind(this), {});

        // Find the oldest iteration for each queue. Revisions landed before a new bot was added are considered fully tested
        // even without results from the not yet existent bot.
        // Unfortunately, we don't know when the bot got added to dashboard, so we have to assume that it was there for as long as it had results.
        var lastTestedRevisionByQueue = {};
        queues.forEach(function(queue) {
            var queueIterations = queue.iterations.filter(function(iteration) { return iteration.finished; });
            queueIterations.sort(function(a, b) { return b.endTime - a.endTime; });
            if (queueIterations.length > 0)
                lastTestedRevisionByQueue[queue.id] = queueIterations[queueIterations.length - 1].openSourceRevision;
        });

        var previousFullyTestedRevisionNumber = -1;

        for (var i = 0; i < iterationsByRevision.length; ++i) {
            var iteration = iterationsByRevision[i];

            console.assert(lastTestedRevisionByQueue[iteration.queue.id] === undefined || lastTestedRevisionByQueue[iteration.queue.id] <= iteration.openSourceRevision);
            lastTestedRevisionByQueue[iteration.queue.id] = iteration.openSourceRevision;

            var newFullyTestedRevisionNumber = this._fullyTestedRevisionNumber(lastTestedRevisionByQueue);
            console.assert(newFullyTestedRevisionNumber >= previousFullyTestedRevisionNumber);

            if (newFullyTestedRevisionNumber === previousFullyTestedRevisionNumber)
                continue;

            for (var revisionNumber = newFullyTestedRevisionNumber; (revisionNumber > previousFullyTestedRevisionNumber) && (revisionNumber in revisions); --revisionNumber) {
                var revision = revisions[revisionNumber];
                console.assert(!("elapsedTime" in revision));
                revision.elapsedTime = iteration.endTime - revision.date;
                revision.elapsedOwnTime = iteration.endTime - iteration.startTime;
            }

            previousFullyTestedRevisionNumber = newFullyTestedRevisionNumber;
        }

        var times = [];
        var ownTimes = [];
        for (var revisionNumber in revisions) {
            var revision = revisions[revisionNumber];
            if (!("elapsedTime" in revision)) {
                // A revision that landed within the time range didn't necessarily get all results by the range end.
                continue;
            }
            // Changes on other branches are irrelevant, as they are not built or tested.
            // FIXME: Support metrics for non-trunk queues.
            if (!revision.containsBranchLocation || revision.branch === "trunk") {
                times.push(revision.elapsedTime);
                ownTimes.push(revision.elapsedOwnTime);
            }
            delete revision.elapsedTime;
            delete revision.elapsedOwnTime;
        }

        result.averageSecondsFromCommit = times.average() / 1000;
        result.medianSecondsFromCommit = times.median() / 1000;
        result.bestSecondsFromCommit = Math.min.apply(Math, times) / 1000;
        result.worstSecondsFromCommit = Math.max.apply(Math, times) / 1000;

        result.averageSecondsOwnTime = ownTimes.average() / 1000;
        result.medianSecondsOwnTime = ownTimes.median() / 1000;
        result.bestSecondsOwnTime = Math.min.apply(Math, ownTimes) / 1000;
        result.worstSecondsOwnTime = Math.max.apply(Math, ownTimes) / 1000;
    },

    _analyzeQueue: function(queue)
    {
        var result = { queueID: queue.id };
        this._countPercentageOfGreen([queue], result);
        this._countTimes([queue], result);

        this.dispatchEventToListeners(Analyzer.Event.QueueResults, result);
    },

    _analyzeAggregate: function()
    {
        var builderQueues = this._queues.filter(function(queue) { return queue.builder; });

        var buildersResult = { queueID: allBuilderResultsPseudoQueue.id };
        this._countPercentageOfGreen(builderQueues, buildersResult);
        this._countTimes(builderQueues, buildersResult);
        this.dispatchEventToListeners(Analyzer.Event.QueueResults, buildersResult);

        var allQueuesResult = { queueID: allResultsPseudoQueue.id };
        this._countPercentageOfGreen(this._queues, allQueuesResult);
        this._countTimes(this._queues, allQueuesResult);
        this.dispatchEventToListeners(Analyzer.Event.QueueResults, allQueuesResult);
    },

    _analyze: function(queue)
    {
        console.assert(this._hasTracData);

        this._queuesReadyToAnalyze.forEach(function(queue) {
            this._analyzeQueue(queue);
        }.bind(this));

        this._queuesReadyToAnalyze = [];

        if (!Object.keys(this._remainingQueues).length) {
            this._analyzeAggregate();
            this.dispatchEventToListeners(Analyzer.Event.Finished, null);
        }
    },

    _loadedFromTrac: function(event)
    {
        this._hasTracData = event.data[0] <= this._rangeStartTime && event.data[1] >= this._rangeEndTime;
        if (this._hasTracData)
            this._analyze();
    },

    _loadedFromBuildbot: function(queue)
    {
        console.assert(this._queueBeingLoaded === queue.id);
        this._queueBeingLoaded = null;

        if (queue.id in this._remainingQueues) {
            delete this._remainingQueues[queue.id];
            this._queuesReadyToAnalyze.push(queue);
        }

        for (var queueID in this._remainingQueues) {
            var queue = this._remainingQueues[queueID];
            console.assert(queue.id === queueID);
            console.assert(!queue.iterations.length);
            this._queueBeingLoaded = queueID;
            queue.loadAll(this._loadedFromBuildbot.bind(this));
            break;
        }

        if (this._hasTracData)
            this._analyze();
    }
};
