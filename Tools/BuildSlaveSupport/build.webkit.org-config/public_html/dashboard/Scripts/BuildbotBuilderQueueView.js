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
            var pendingBuildCount = queue.pendingIterationsCount;
            if (pendingBuildCount) {
                var message = pendingBuildCount === 1 ? "pending build" : "pending builds";
                var status = new StatusLineView(message, StatusLineView.Status.Neutral, null, pendingBuildCount);
                this.element.appendChild(status.element);
            }

            var firstRecentFailedIteration = queue.firstRecentFailedIteration;
            if (firstRecentFailedIteration && firstRecentFailedIteration.loaded) {
                var failureCount = queue.recentFailedIterationCount;
                var message = this.revisionLinksForIteration(firstRecentFailedIteration);
                var url = queue.buildbot.buildLogURLForIteration(firstRecentFailedIteration);
                var status = new StatusLineView(message, StatusLineView.Status.Bad, failureCount > 1 ? "failed builds since" : "failed build", failureCount > 1 ? failureCount : null, url);
                this.element.appendChild(status.element);
            }

            var mostRecentSuccessfulIteration = queue.mostRecentSuccessfulIteration;
            if (mostRecentSuccessfulIteration && mostRecentSuccessfulIteration.loaded) {
                var message = this.revisionLinksForIteration(mostRecentSuccessfulIteration);
                var url = queue.buildbot.buildLogURLForIteration(mostRecentSuccessfulIteration);
                var status = new StatusLineView(message, StatusLineView.Status.Good, firstRecentFailedIteration ? "last successful build" : "latest build", null, url);
                this.element.appendChild(status.element);
            } else {
                var status = new StatusLineView("unknown", StatusLineView.Status.Neutral, firstRecentFailedIteration ? "last successful build" : "latest build");            
                this.element.appendChild(status.element);

                if (firstRecentFailedIteration) {
                    // We have a failed iteration but no successful. It might be further back in time.
                    // Update all the iterations so we get more history.
                    queue.iterations.forEach(function(iteration) { iteration.update(); });
                }
            }
        }

        function appendBuildArchitecture(queues, label)
        {
            if (!queues.length)
                return;

            var releaseLabel = document.createElement("label");
            releaseLabel.textContent = label;
            this.element.appendChild(releaseLabel);

            queues.forEach(appendBuilderQueueStatus.bind(this));
        }

        appendBuildArchitecture.call(this, this.universalReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (Universal)" : "Release");
        appendBuildArchitecture.call(this, this.sixtyFourBitReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (64-bit)" : "Release");
        appendBuildArchitecture.call(this, this.thirtyTwoBitReleaseQueues, this.hasMultipleReleaseBuilds ? "Release (32-bit)" : "Release");

        appendBuildArchitecture.call(this, this.universalDebugQueues, this.hasMultipleDebugBuilds ? "Debug (Universal)" : "Debug");
        appendBuildArchitecture.call(this, this.sixtyFourBitDebugQueues, this.hasMultipleDebugBuilds ? "Debug (64-bit)" : "Debug");
        appendBuildArchitecture.call(this, this.thirtyTwoBitDebugQueues, this.hasMultipleDebugBuilds ? "Debug (32-bit)" : "Debug");
    }
};
