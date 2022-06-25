/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

BuildbotPerformanceQueueView = function(queues)
{
    BuildbotQueueView.call(this, queues);
    this.update();
};

BaseObject.addConstructorFunctions(BuildbotPerformanceQueueView);

BuildbotPerformanceQueueView.prototype = {
    constructor: BuildbotPerformanceQueueView,
    __proto__: BuildbotQueueView.prototype,

    update: function()
    {
        QueueView.prototype.update.call(this);

        this.element.removeChildren();

        function appendPerformanceQueueStatus(queue)
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

                if (iteration.successful) {
                    var status = new StatusLineView(messageElement, StatusLineView.Status.Good, "all tests passed");
                    limit = 0;
                } else if (!iteration.productive) {
                    var url = iteration.queue.buildbot.buildPageURLForIteration(iteration);
                    var status = new StatusLineView(messageElement, StatusLineView.Status.Danger, iteration.text, null, url);
                } else if (iteration.failed) {
                    var url = iteration.queue.buildbot.buildPageURLForIteration(iteration);
                    var status = new StatusLineView(messageElement, StatusLineView.Status.Bad, iteration.text, null, url);
                } else {
                    var url = iteration.queue.buildbot.buildPageURLForIteration(iteration);
                    var status = new StatusLineView(messageElement, StatusLineView.Status.Danger, iteration.text, null, url);
                }

                this.element.appendChild(status.element);
                appendedStatus = true;
            }

            if (!appendedStatus) {
                var status = new StatusLineView("unknown", StatusLineView.Status.Neutral, "last passing build");
                this.element.appendChild(status.element);
            }

            new PopoverTracker(status.statusBubbleElement, this._presentPopoverForPerformanceQueue.bind(this), queue);
        }

        this.appendBuildStyle.call(this, this.queues, 'Release', appendPerformanceQueueStatus);
    },

    addLinkToRow: function(rowElement, className, text, url)
    {
        var linkElement = document.createElement("a");
        linkElement.className = className;
        linkElement.textContent = text;
        linkElement.href = url;
        linkElement.target = "_blank";
        rowElement.appendChild(linkElement);
    },

    _popoverContentForPerformanceQueue: function(queue)
    {
        var content = document.createElement("div");
        content.className = "performance-popover";

        var row = document.createElement("div");
        this.addLinkToRow(row, "dashboard-link", "Performance Dashboard", queue.buildbot.performanceDashboardURL);

        content.appendChild(row);

        return content;
    },

    _presentPopoverForPerformanceQueue: function(element, popover, queue)
    {
        var content = this._popoverContentForPerformanceQueue(queue);

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);
        return true;
    }
};
