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

BuildbotLeaksQueueView = function(queues)
{
    BuildbotQueueView.call(this, queues);
    this.update();
};

BaseObject.addConstructorFunctions(BuildbotLeaksQueueView);

BuildbotLeaksQueueView.prototype = {
    constructor: BuildbotLeaksQueueView,
    __proto__: BuildbotQueueView.prototype,

    update: function()
    {
        QueueView.prototype.update.call(this);

        this.element.removeChildren();

        function appendLeaksQueueStatus(queue)
        {
            var appendedStatus = false;

            for (var i = 0; i < queue.iterations.length; ++i) {
                var iteration = queue.iterations[i];
                if (!iteration.loaded || !iteration.finished)
                    continue;

                var messageElement = this.revisionContentForIteration(iteration, null);

                var buildPageURL = iteration.queue.buildbot.buildPageURLForIteration(iteration);

                if (iteration.successful)
                    var status = new StatusLineView(messageElement, StatusLineView.Status.Good, "no leaks", null, buildPageURL);
                else if (!iteration.productive)
                    var status = new StatusLineView(messageElement, StatusLineView.Status.Danger, iteration.text, null, buildPageURL);
                else if (iteration.failed) {
                    var url = iteration.queue.buildbot.buildPageURLForIteration(iteration);
                    var leakCount = iteration.layoutTestResults.uniqueLeakCount;
                    if (leakCount) {
                        var leaksViewerURL = iteration.resultURLs["view leaks"];
                        var status = new StatusLineView(messageElement, StatusLineView.Status.Neutral, leakCount === 1 ? "unique leak" : "unique leaks", leakCount, leaksViewerURL);
                    } else
                        var status = new StatusLineView(messageElement, StatusLineView.Status.Danger, iteration.text, null, buildPageURL);
                } else {
                    var url = iteration.queue.buildbot.buildPageURLForIteration(iteration);
                    var status = new StatusLineView(messageElement, StatusLineView.Status.Danger, iteration.text, null, buildPageURL);
                }

                this.element.appendChild(status.element);
                appendedStatus = true;
                break;
            }

            if (!appendedStatus) {
                var status = new StatusLineView("unknown", StatusLineView.Status.Neutral, "last passing build");
                this.element.appendChild(status.element);
            }

            new PopoverTracker(status.statusBubbleElement, this._presentPopoverForLeaksQueue.bind(this), iteration);
        }

        this.appendBuildStyle.call(this, this.queues, "Leaks", appendLeaksQueueStatus);
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

    _popoverContentForLeaksQueue: function(iteration)
    {
        var content = document.createElement("div");

        content.className = "leaks-popover";

        this._addIterationHeadingToPopover(iteration, content, "");
        this._addDividerToPopover(content);

        var row = document.createElement("div");
        row.textContent = iteration.text;
        content.appendChild(row);

        this._addDividerToPopover(content);

        for (var linkName in iteration.resultURLs) {
            var row = document.createElement("div");
            this.addLinkToRow(row, "result-link", linkName, iteration.resultURLs[linkName]);
            content.appendChild(row);
        }

        return content;
    },

    _presentPopoverForLeaksQueue: function(element, popover, iteration)
    {
        var content = this._popoverContentForLeaksQueue(iteration);

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);
        return true;
    }

};
