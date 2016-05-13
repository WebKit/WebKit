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

BuildbotBuilderQueueView = function(queues)
{
    BuildbotQueueView.call(this, queues);

    function filterQueues(architecture, debug, queue)
    {
        return queue.architecture === architecture && queue.debug === debug;
    }

    this.universalReleaseQueues = this.queues.filter(filterQueues.bind(this, Buildbot.BuildArchitecture.Universal, false));
    this.sixtyFourBitReleaseQueues = this.queues.filter(filterQueues.bind(this, Buildbot.BuildArchitecture.SixtyFourBit, false));
    this.thirtyTwoBitReleaseQueues = this.queues.filter(filterQueues.bind(this, Buildbot.BuildArchitecture.ThirtyTwoBit, false));

    this.universalDebugQueues = this.queues.filter(filterQueues.bind(this, Buildbot.BuildArchitecture.Universal, true));
    this.sixtyFourBitDebugQueues = this.queues.filter(filterQueues.bind(this, Buildbot.BuildArchitecture.SixtyFourBit, true));
    this.thirtyTwoBitDebugQueues = this.queues.filter(filterQueues.bind(this, Buildbot.BuildArchitecture.ThirtyTwoBit, true));

    this.hasMultipleReleaseBuilds = this.universalReleaseQueues.length + this.sixtyFourBitReleaseQueues.length + this.thirtyTwoBitReleaseQueues.length > 1;
    this.hasMultipleDebugBuilds = this.universalDebugQueues.length + this.sixtyFourBitDebugQueues.length + this.thirtyTwoBitDebugQueues.length > 1;

    this.update();
};

BaseObject.addConstructorFunctions(BuildbotBuilderQueueView);

BuildbotBuilderQueueView.prototype = {
    constructor: BuildbotBuilderQueueView,
    __proto__: BuildbotQueueView.prototype,

    update: function()
    {
        BuildbotQueueView.prototype.update.call(this);

        this.element.removeChildren();

        function appendBuilderQueueStatus(queue)
        {
            if (queue.buildbot.needsAuthentication && !queue.buildbot.isAuthenticated) {
                this._appendUnauthorizedLineView(queue);
                return;
            }

            this._appendPendingRevisionCount(queue, this._latestProductiveIteration.bind(this, queue));

            var firstRecentUnsuccessfulIteration = queue.firstRecentUnsuccessfulIteration;
            var mostRecentFinishedIteration = queue.mostRecentFinishedIteration;
            var mostRecentSuccessfulIteration = queue.mostRecentSuccessfulIteration;

            if (firstRecentUnsuccessfulIteration && firstRecentUnsuccessfulIteration.loaded
                && mostRecentFinishedIteration && mostRecentFinishedIteration.loaded) {
                console.assert(!mostRecentFinishedIteration.successful);
                var message = this.revisionContentForIteration(mostRecentFinishedIteration, mostRecentFinishedIteration.productive ? mostRecentSuccessfulIteration : null);
                if (mostRecentFinishedIteration.failed && mostRecentFinishedIteration.productive) {
                    // Assume it was a build step that failed, and link directly to output.
                    var url = mostRecentFinishedIteration.failureLogURL("build log");
                    if (!url)
                        url = mostRecentFinishedIteration.failureLogURL("stdio");
                    var status = StatusLineView.Status.Bad;
                } else
                    var status = StatusLineView.Status.Danger;

                // Show a popover when the URL is not a main build page one, because there are usually multiple logs, and it's good to provide a choice.
                var needsPopover = !!url;

                // Some other step failed, link to main buildbot page for the iteration.
                if (!url)
                    url = queue.buildbot.buildPageURLForIteration(mostRecentFinishedIteration);
                var status = new StatusLineView(message, status, mostRecentFinishedIteration.text, null, url);
                this.element.appendChild(status.element);

                if (needsPopover)
                    new PopoverTracker(status.statusBubbleElement, this._presentPopoverFailureLogs.bind(this), mostRecentFinishedIteration);
            }

            if (mostRecentSuccessfulIteration && mostRecentSuccessfulIteration.loaded) {
                var message = this.revisionContentForIteration(mostRecentSuccessfulIteration);
                var url = queue.buildbot.buildPageURLForIteration(mostRecentSuccessfulIteration);
                var status = new StatusLineView(message, StatusLineView.Status.Good, firstRecentUnsuccessfulIteration ? "last successful build" : "latest build", null, url);
                this.element.appendChild(status.element);
            } else {
                var status = new StatusLineView("unknown", StatusLineView.Status.Neutral, firstRecentUnsuccessfulIteration ? "last successful build" : "latest build");
                this.element.appendChild(status.element);

                if (firstRecentUnsuccessfulIteration) {
                    // We have a failed iteration but no successful. It might be further back in time.
                    queue.loadMoreHistoricalIterations();
                }
            }
        }

        this.appendBuildStyle.call(this, this.universalReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (Universal)" : "Release", appendBuilderQueueStatus);
        this.appendBuildStyle.call(this, this.sixtyFourBitReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (64-bit)" : "Release", appendBuilderQueueStatus);
        this.appendBuildStyle.call(this, this.thirtyTwoBitReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (32-bit)" : "Release", appendBuilderQueueStatus);

        this.appendBuildStyle.call(this, this.universalDebugQueues, this.hasMultipleDebugBuilds ? "Debug (Universal)" : "Debug", appendBuilderQueueStatus);
        this.appendBuildStyle.call(this, this.sixtyFourBitDebugQueues, this.hasMultipleDebugBuilds ? "Debug (64-bit)" : "Debug", appendBuilderQueueStatus);
        this.appendBuildStyle.call(this, this.thirtyTwoBitDebugQueues, this.hasMultipleDebugBuilds ? "Debug (32-bit)" : "Debug", appendBuilderQueueStatus);
    },

    _presentPopoverFailureLogs: function(element, popover, iteration)
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
    }
};
