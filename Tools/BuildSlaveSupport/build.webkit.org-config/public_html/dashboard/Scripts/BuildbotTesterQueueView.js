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

BuildbotTesterQueueView = function(debugQueues, releaseQueues)
{
    BuildbotQueueView.call(this, debugQueues, releaseQueues);

    this.update();
};

BaseObject.addConstructorFunctions(BuildbotTesterQueueView);

BuildbotTesterQueueView.prototype = {
    constructor: BuildbotTesterQueueView,
    __proto__: BuildbotQueueView.prototype,

    update: function()
    {
        BuildbotQueueView.prototype.update.call(this);

        this.element.removeChildren();

        function appendBuilderQueueStatus(queue)
        {
            var pendingRunsCount = queue.pendingIterationsCount;
            if (pendingRunsCount) {
                var message = pendingRunsCount === 1 ? "pending test run" : "pending test runs";
                var status = new StatusLineView(message, StatusLineView.Status.Neutral, null, pendingRunsCount);
                this.element.appendChild(status.element);
            }

            var appendedStatus = false;

            var limit = 2;
            for (var i = 0; i < queue.iterations.length && limit > 0; ++i) {
                var iteration = queue.iterations[i];
                if (!iteration.loaded || !iteration.finished)
                    continue;

                --limit;

                var messageLinkElement = this.revisionLinksForIteration(iteration);

                var layoutTestResults = iteration.layoutTestResults || {};
                var javascriptTestResults = iteration.javascriptTestResults || {};
                var pythonTestResults = iteration.pythonTestResults || {};
                var perlTestResults = iteration.perlTestResults || {};
                var bindingTestResults = iteration.bindingTestResults || {};

                if (!layoutTestResults.failureCount && !javascriptTestResults.failureCount && !pythonTestResults.failureCount && !perlTestResults.failureCount && !bindingTestResults.failureCount) {
                    var status = new StatusLineView(messageLinkElement, StatusLineView.Status.Good, "all tests passed");
                    limit = 0;
                } else if (layoutTestResults.failureCount && !javascriptTestResults.failureCount && !pythonTestResults.failureCount && !perlTestResults.failureCount && !bindingTestResults.failureCount) {
                    var url = iteration.queue.buildbot.layoutTestResultsURLForIteration(iteration);
                    var status = new StatusLineView(messageLinkElement, StatusLineView.Status.Bad, layoutTestResults.failureCount === 1 ? "layout test failure" : "layout test failures", layoutTestResults.tooManyFailures ? layoutTestResults.failureCount + "\uff0b" : layoutTestResults.failureCount, url);
                } else if (!layoutTestResults.failureCount && javascriptTestResults.failureCount && !pythonTestResults.failureCount && !perlTestResults.failureCount && !bindingTestResults.failureCount) {
                    var url = iteration.queue.buildbot.javascriptTestResultsURLForIteration(iteration);
                    var status = new StatusLineView(messageLinkElement, StatusLineView.Status.Bad, javascriptTestResults.failureCount === 1 ? "javascript test failure" : "javascript test failures", javascriptTestResults.failureCount, url);
                } else if (!layoutTestResults.failureCount && !javascriptTestResults.failureCount && pythonTestResults.failureCount && !perlTestResults.failureCount && !bindingTestResults.failureCount) {
                    var status = new StatusLineView(messageLinkElement, StatusLineView.Status.Bad, pythonTestResults.failureCount === 1 ? "webkitpy test failure" : "webkitpy test failures", pythonTestResults.failureCount);
                } else if (!layoutTestResults.failureCount && !javascriptTestResults.failureCount && !pythonTestResults.failureCount && perlTestResults.failureCount && !bindingTestResults.failureCount) {
                    var status = new StatusLineView(messageLinkElement, StatusLineView.Status.Bad, perlTestResults.failureCount === 1 ? "webkitperl test failure" : "webkitperl test failures", perlTestResults.failureCount);
                } else if (!layoutTestResults.failureCount && !javascriptTestResults.failureCount && !pythonTestResults.failureCount && !perlTestResults.failureCount && bindingTestResults.failureCount) {
                    var status = new StatusLineView(messageLinkElement, StatusLineView.Status.Bad, bindingTestResults.failureCount === 1 ? "binding test failure" : "binding test failures", bindingTestResults.failureCount);
                } else {
                    var totalFailures = layoutTestResults.failureCount + javascriptTestResults.failureCount + pythonTestResults.failureCount + perlTestResults.failureCount + bindingTestResults.failureCount;
                    var status = new StatusLineView(messageLinkElement, StatusLineView.Status.Bad, totalFailures === 1 ? "test failure" : "test failures");
                }

                this.element.appendChild(status.element);
                appendedStatus = true;
            }

            if (!appendedStatus) {
                var status = new StatusLineView("unknown", StatusLineView.Status.Neutral, "last passing build");            
                this.element.appendChild(status.element);
            }
        }

        function appendBuild(queues, label)
        {
            if (!queues.length)
                return;

            var releaseLabel = document.createElement("label");
            releaseLabel.textContent = label;
            this.element.appendChild(releaseLabel);

            queues.forEach(appendBuilderQueueStatus.bind(this));
        }

        appendBuild.call(this, this.releaseQueues, "Release");
        appendBuild.call(this, this.debugQueues, "Debug");
    },
};
