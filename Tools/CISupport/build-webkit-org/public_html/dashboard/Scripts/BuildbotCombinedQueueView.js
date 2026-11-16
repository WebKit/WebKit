/*
 * Copyright (C) 2013, 2015-2016 Apple Inc. All rights reserved.
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

BuildbotCombinedQueueView = function(queue)
{
    console.assert(queue.branches === undefined);
    var indicesOfFirstQueueWithRepository = {};
    var combinedQueues = queue.combinedQueues;
    var buildbot = combinedQueues[0].buildbot;
    for (var i = 0; i < combinedQueues.length; ++i) {
        var subQueue = combinedQueues[i];
        console.assert(buildbot === subQueue.buildbot);
        var branches = subQueue.branches;
        for (var j = 0; j < branches.length; ++j) {
            var branch = branches[j];
            var repositoryName = branch.repository.name;
            var expected = indicesOfFirstQueueWithRepository[repositoryName];
            if (expected === undefined) {
                indicesOfFirstQueueWithRepository[repositoryName] = { queueIndex: i, branchIndex: j };
                continue;
            }
            var expectedBranch = combinedQueues[expected.queueIndex].branches[expected.branchIndex];
            var message = queue.id + ": " + branch.name + " === combinedQueues[" + i + "].branch[" + j + "] ";
            message += "=== combinedQueues[" + expected.queueIndex + "].branch[" + expected.branchIndex + "] === " + expectedBranch.name;
            console.assert(branch.name === expectedBranch.name, message);
        }
    }

    BuildbotQueueView.call(this, queue.combinedQueues);

    this._alwaysExpand = false;
    this.combinedQueue = queue;
    this.update();
};

BaseObject.addConstructorFunctions(BuildbotCombinedQueueView);

BuildbotCombinedQueueView.prototype = {
    constructor: BuildbotCombinedQueueView,
    __proto__: BuildbotQueueView.prototype,

    update: function()
    {
        BuildbotQueueView.prototype.update.call(this);

        this.element.removeChildren();

        if (!this._alwaysExpand && this._queuesShouldDisplayCombined()) {
            var releaseLabel = document.createElement("a");
            releaseLabel.classList.add("queueLabel");
            releaseLabel.href = "#";
            releaseLabel.textContent = this.combinedQueue.heading;
            releaseLabel.onclick = function() { this._alwaysExpand = true; this.update(); return false; }.bind(this);
            this.element.appendChild(releaseLabel);

            var queue = this.queues[0]; // All queues in the combined queue are from the same buildbot.
            if (queue.buildbot.needsAuthentication && !queue.buildbot.isAuthenticated) {
                this._appendUnauthorizedLineView(queue);
                return;
            }

            // Show the revision for the slowest queue, because we don't know if any newer revisions are green on all queues.
            // This can be slightly misleading after fixing a problem, because we can show a known broken revision as green.
            var slowestQueue = this.queues.slice().sort(function(a, b) { return BuildbotQueue.prototype.compareIterationsByRevisions(a.mostRecentSuccessfulIteration, b.mostRecentSuccessfulIteration); }).pop();
            this._appendPendingRevisionCount(slowestQueue, this._latestProductiveIteration.bind(this, slowestQueue));

            var message = this.revisionContentForIteration(slowestQueue.mostRecentSuccessfulIteration);
            var statusMessagePassed = "all " + (queue.builder ? "builds succeeded" : "tests passed");
            var statusView = new StatusLineView(message, StatusLineView.Status.Good, statusMessagePassed, null, null);
            new PopoverTracker(statusView.statusBubbleElement, this._presentPopoverForCombinedGreenBubble.bind(this));
            this.element.appendChild(statusView.element);
        } else {
            this.appendBuildStyle.call(this, this.queues, null, function(queue) {
                if (queue.buildbot.needsAuthentication && !queue.buildbot.isAuthenticated) {
                    this._appendUnauthorizedLineView(queue);
                    return;
                }

                this._appendPendingRevisionCount(queue, this._latestProductiveIteration.bind(this, queue));

                var firstRecentUnsuccessfulIteration = queue.firstRecentUnsuccessfulIteration;
                var mostRecentFinishedIteration = queue.mostRecentFinishedIteration;
                var mostRecentSuccessfulIteration = queue.mostRecentSuccessfulIteration;

                if (firstRecentUnsuccessfulIteration && firstRecentUnsuccessfulIteration.loaded && mostRecentFinishedIteration && mostRecentFinishedIteration.loaded) {
                    console.assert(!mostRecentFinishedIteration.successful);
                    var message = this.revisionContentForIteration(mostRecentFinishedIteration, mostRecentFinishedIteration.productive ? mostRecentSuccessfulIteration : null);
                    if (!mostRecentFinishedIteration.productive) {
                        var status = StatusLineView.Status.Danger;
                    } else if (mostRecentFinishedIteration.failedTestSteps.length === 1 && /(?=.*test)(?=.*jsc)/.test(mostRecentFinishedIteration.failedTestSteps[0].name)) {
                        var failedStep = mostRecentFinishedIteration.failedTestSteps[0];
                        var URL = mostRecentFinishedIteration.queue.buildbot.javaScriptCoreTestStdioUrlForIteration(mostRecentFinishedIteration, failedStep.name);
                        var statusView = new StatusLineView(message, StatusLineView.Status.Bad, this._testStepFailureDescription(failedStep), failedStep.failureCount, URL);
                        this.element.appendChild(statusView.element);
                        new PopoverTracker(statusView.statusBubbleElement, this._presentPopoverForJavaScriptCoreTestRegressions.bind(this, failedStep.name), mostRecentFinishedIteration);
                        return;
                    } else {
                        // Direct links to some common logs.
                        var url = mostRecentFinishedIteration.failureLogURL("build log");
                        if (!url)
                            url = mostRecentFinishedIteration.failureLogURL("stdio");
                        var status = StatusLineView.Status.Bad;
                    }

                    // Show a popover when the URL is not a main build page one, because there are usually multiple logs, and it's good to provide a choice.
                    var needsPopover = !url;

                    // Some other step failed, link to main buildbot page for the iteration.
                    if (!url)
                        url = queue.buildbot.buildPageURLForIteration(mostRecentFinishedIteration);
                    var statusView = new StatusLineView(message, status, mostRecentFinishedIteration.text, null, url);
                    this.element.appendChild(statusView.element);

                    if (needsPopover)
                        new PopoverTracker(statusView.statusBubbleElement, this._presentIndividualQueuePopover.bind(this), mostRecentFinishedIteration);
                }

                var statusMessagePassed = "all " + (queue.builder ? "builds succeeded" : "tests passed");
                if (mostRecentSuccessfulIteration && mostRecentSuccessfulIteration.loaded) {
                    var message = this.revisionContentForIteration(mostRecentSuccessfulIteration);
                    var url = queue.buildbot.buildPageURLForIteration(mostRecentSuccessfulIteration);
                    var label = firstRecentUnsuccessfulIteration ? "last succeeded" : statusMessagePassed;
                    var statusView = new StatusLineView(message, StatusLineView.Status.Good, label, null, url);
                    this.element.appendChild(statusView.element);
                } else {
                    var label = firstRecentUnsuccessfulIteration ? "last succeeded" : statusMessagePassed;
                    var statusView = new StatusLineView("unknown", StatusLineView.Status.Neutral, label);
                    this.element.appendChild(statusView.element);

                    if (firstRecentUnsuccessfulIteration) {
                        // We have a failed iteration but no successful. It might be further back in time.
                        queue.loadMoreHistoricalIterations();
                    }
                }
            });
        }
    },

    // All queues are green, or all are unauthorized (the latter case always applies to all queues, because they are all from the same buildbot).
    _queuesShouldDisplayCombined: function()
    {
        for (var i = 0, end = this.queues.length; i < end; ++i) {
            var queue = this.queues[i];
            if (queue.buildbot.needsAuthentication && !queue.buildbot.isAuthenticated)
                return true;
            if (!queue.mostRecentFinishedIteration || !queue.mostRecentFinishedIteration.successful)
                return false;
        }
        return true;
    },

    _presentPopoverForCombinedGreenBubble: function(element, popover)
    {
        var content = document.createElement("div");
        content.className = "combined-queue-popover";

        var title = document.createElement("div");
        title.className = "popover-iteration-heading";
        title.textContent = "latest tested revisions";
        content.appendChild(title);

        this._addDividerToPopover(content);

        function addQueue(queue, view) {
            var line = document.createElement("div");
            var link = document.createElement("a");
            link.className = "queue-link";
            link.href = queue.overviewURL;
            link.textContent = queue.heading;
            link.target = "_blank";
            line.appendChild(link);
            var revision = document.createElement("span");
            revision.className = "revision";
            revision.appendChild(view.revisionContentForIteration(queue.mostRecentSuccessfulIteration));
            line.appendChild(revision);
            content.appendChild(line);
        }

        for (var i = 0, end = this.queues.length; i < end; ++i)
            addQueue(this.queues[i], this);

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);
        return true;
    },

    _presentIndividualQueuePopover: function(element, popover, iteration)
    {
        var content = document.createElement("div");
        content.className = "build-logs-popover";

        function addLog(name, url) {
            var line = document.createElement("a");
            line.className = "build-log-link";
            line.href = url;
            line.textContent = name;
            line.target = "_blank";
            content.appendChild(line);
        }

        this._addIterationHeadingToPopover(iteration, content);
        this._addDividerToPopover(content);
        
        var logsHeadingLine = document.createElement("div");
        logsHeadingLine.className = "build-logs-heading";
        logsHeadingLine.textContent = iteration.firstFailedStepName + " failed";
        content.appendChild(logsHeadingLine);

        for (var i = 0, end = iteration.failureLogs.length; i < end; ++i)
            addLog(iteration.failureLogs[i][0], iteration.failureLogs[i][1]);

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);
        return true;
    },
};
