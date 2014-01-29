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

BuildbotBuilderQueueView = function(debugQueues, releaseQueues)
{
    BuildbotQueueView.call(this, debugQueues, releaseQueues);

    function filterQueuesByArchitecture(architecture, queue)
    {
        return queue.architecture === architecture;
    }

    this.universalReleaseQueues = this.releaseQueues.filter(filterQueuesByArchitecture.bind(this, Buildbot.BuildArchitecture.Universal));
    this.sixtyFourBitReleaseQueues = this.releaseQueues.filter(filterQueuesByArchitecture.bind(this, Buildbot.BuildArchitecture.SixtyFourBit));
    this.thirtyTwoBitReleaseQueues = this.releaseQueues.filter(filterQueuesByArchitecture.bind(this, Buildbot.BuildArchitecture.ThirtyTwoBit));

    this.universalDebugQueues = this.debugQueues.filter(filterQueuesByArchitecture.bind(this, Buildbot.BuildArchitecture.Universal));
    this.sixtyFourBitDebugQueues = this.debugQueues.filter(filterQueuesByArchitecture.bind(this, Buildbot.BuildArchitecture.SixtyFourBit));
    this.thirtyTwoBitDebugQueues = this.debugQueues.filter(filterQueuesByArchitecture.bind(this, Buildbot.BuildArchitecture.ThirtyTwoBit));

    this.hasMultipleReleaseBuilds = this.releaseQueues.length > 1;
    this.hasMultipleDebugBuilds = this.debugQueues.length > 1;

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
            this._appendPendingRevisionCount(queue);

            var firstRecentUnsuccessfulIteration = queue.firstRecentUnsuccessfulIteration;
            var mostRecentFinishedIteration = queue.mostRecentFinishedIteration;
            var mostRecentSuccessfulIteration = queue.mostRecentSuccessfulIteration;

            if (firstRecentUnsuccessfulIteration && firstRecentUnsuccessfulIteration.loaded
                && mostRecentFinishedIteration && mostRecentFinishedIteration.loaded) {
                console.assert(!mostRecentFinishedIteration.successful);
                var message = this.revisionContentForIteration(mostRecentFinishedIteration, mostRecentFinishedIteration.productive ? mostRecentSuccessfulIteration : null);
                if (mostRecentFinishedIteration.failed) {
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
                    // Update all the iterations so we get more history.
                    // FIXME: It can be very time consuming to load all iterations, we should load progressively.
                    queue.iterations.forEach(function(iteration) { iteration.update(); });
                }
            }
        }

        function appendBuildArchitecture(queues, label)
        {
            queues.forEach(function(queue) {
                var releaseLabel = document.createElement("a");
                releaseLabel.classList.add("queueLabel");
                releaseLabel.textContent = label;
                releaseLabel.href = queue.overviewURL;
                releaseLabel.target = "_blank";
                this.element.appendChild(releaseLabel);

                appendBuilderQueueStatus.call(this, queue);
            }.bind(this));
        }

        appendBuildArchitecture.call(this, this.universalReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (Universal)" : "Release");
        appendBuildArchitecture.call(this, this.sixtyFourBitReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (64-bit)" : "Release");
        appendBuildArchitecture.call(this, this.thirtyTwoBitReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (32-bit)" : "Release");

        appendBuildArchitecture.call(this, this.universalDebugQueues, this.hasMultipleDebugBuilds ? "Debug (Universal)" : "Debug");
        appendBuildArchitecture.call(this, this.sixtyFourBitDebugQueues, this.hasMultipleDebugBuilds ? "Debug (64-bit)" : "Debug");
        appendBuildArchitecture.call(this, this.thirtyTwoBitDebugQueues, this.hasMultipleDebugBuilds ? "Debug (32-bit)" : "Debug");
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
