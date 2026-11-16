/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

BuildbotStaticAnalyzerQueueView = function(queues)
{
    BuildbotQueueView.call(this, queues);
    this.update();
};

BaseObject.addConstructorFunctions(BuildbotStaticAnalyzerQueueView);

BuildbotStaticAnalyzerQueueView.prototype = {
    constructor: BuildbotStaticAnalyzerQueueView,
    __proto__: BuildbotQueueView.prototype,

    update: function()
    {
        QueueView.prototype.update.call(this);

        this.element.removeChildren();

        function appendStaticAnalyzerQueueStatus(queue)
        {
            var appendedStatus = false;

            var limit = 2;
            for (var i = 0; i < queue.iterations.length && limit > 0; ++i) {
                var iteration = queue.iterations[i];
                if (!iteration.loaded || !iteration.finished)
                    continue;

                --limit;

                var willHaveAnotherStatusLine = i + 1 < queue.iterations.length && limit > 0 && !iteration.successful; // This is not 100% correct, as the remaining iterations may not be finished or loaded yet, but close enough.
                var messageElement = this.revisionContentForIteration(iteration, (iteration.productive && willHaveAnotherStatusLine) ? iteration.previousProductiveIteration : null);

                var url = iteration.queue.buildbot.layoutTestResultsURLForIteration(iteration);
                var statusLineViewColor = StatusLineView.Status.Neutral;
                var text;

                var failedStep = new BuildbotTestResults(iteration._firstFailedStep);
                var issueCount = failedStep.issueCount;

                if (iteration.successful) {
                    statusLineViewColor = StatusLineView.Status.Good;
                    limit = 0;
                    text = "no issues found";
                } else if (!iteration.productive) {
                    statusLineViewColor = StatusLineView.Status.Danger;
                    text = iteration._firstFailedStep.name + " failed";
                } else if (iteration.failed && /failed to build/.test(iteration.text)) {
                    url = iteration.queue.buildbot.buildPageURLForIteration(iteration);
                    statusLineViewColor = StatusLineView.Status.Bad;
                    text = "build failed";
                    issueCount = null;
                }

                var status = new StatusLineView(messageElement, statusLineViewColor, (text) ? text : "found " + failedStep.issueCount + " issues", issueCount, url);

                this.element.appendChild(status.element);
                appendedStatus = true;
            }

            if (!appendedStatus) {
                var status = new StatusLineView("unknown", StatusLineView.Status.Neutral, "last passing build");
                this.element.appendChild(status.element);
            }
        }

        this.appendBuildStyle.call(this, this.queues, 'Static Analyzer', appendStaticAnalyzerQueueStatus);
    }
};
