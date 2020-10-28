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

BubbleQueueView = function(queues)
{
    QueueView.call(this);

    this.queues = queues || [];

    this.queues.forEach(function(queue) {
        if (this.platform && this.platform != queue.platform)
            throw "A bubble queue view may not contain queues for multiple platforms."
        else
            this.platform = queue.platform;
        queue.addEventListener(BubbleQueue.Event.Updated, this._queueUpdated, this);
    }.bind(this));

    this.update();
};

BaseObject.addConstructorFunctions(BubbleQueueView);

BubbleQueueView.prototype = {
    constructor: BubbleQueueView,
    __proto__: QueueView.prototype,

    update: function()
    {
        QueueView.prototype.update.call(this);

        this.element.removeChildren();

        function appendQueue(queue)
        {
            var queueLabel = document.createElement("a");
            queueLabel.classList.add("queueLabel");
            queueLabel.textContent = queue.title;
            queueLabel.href = queue.statusPageURL;
            queueLabel.target = "_blank";
            this.element.appendChild(queueLabel);

            var patchCount = queue.patchCount;

            var message = patchCount === 1 ? "patch in queue" : "patches in queue";
            var status = new StatusLineView(message, StatusLineView.Status.Neutral, null, patchCount || "0");
            this.element.appendChild(status.element);

            new PopoverTracker(status.statusBubbleElement, this._presentPopoverForBubbleQueue.bind(this), queue);
        }

        this.queues.forEach(function(queue) {
            appendQueue.call(this, queue);
        }, this);
    },

    _addQueueHeadingToPopover: function(queue, content)
    {
        var title = document.createElement("div");
        title.className = "popover-queue-heading";

        this.addTextToRow(title, "queue-name", queue.id);
        this.addLinkToRow(title, "queue-status-link", "status page", queue.statusPageURL);
        this.addLinkToRow(title, "queue-charts-link", "charts", queue.chartsPageURL);

        content.appendChild(title);
    },

    _addBotsHeadingToPopover: function(queue, content)
    {
        var title = document.createElement("div");
        title.className = "popover-bots-heading";
        title.textContent = "latest bot event";
        content.appendChild(title);
    },

    _timeIntervalString: function(time)
    {
        var timeDifference = (Date.now() - time.getTime()) / 1000;
        return this._readableTimeString(timeDifference)
    },

    _popoverContentForBubbleQueue: function(queue)
    {
        var content = document.createElement("div");
        content.className = "bubble-server-popover";

        this._addQueueHeadingToPopover(queue, content);
        this._addDividerToPopover(content);

        var patches = queue.patches;
        for (var i = 0, end = patches.length; i < end; ++i) {
            var patch = patches[i];

            var rowElement = document.createElement("div");

            this.addLinkToRow(rowElement, "patch-details-link", patch.attachmentID, patch.statusPageURL);

            if (patch.retryCount)
                this.addTextToRow(rowElement, "failure-count", patch.retryCount + "\xa0" + (patch.retryCount === 1 ? "attempt" : "attempts"));

            if (patch.detailedResultsURLForLatestMessage)
                this.addLinkToRow(rowElement, "latest-status-with-link", patch.latestMessage, patch.detailedResultsURLForLatestMessage);
            else if (patch.latestMessage && patch.latestMessage.length)
                this.addTextToRow(rowElement, "latest-status-no-link", patch.latestMessage);
            else if (patch.active) {
                this.addTextToRow(rowElement, "latest-status-no-link", "Started");
                this.addTextToRow(rowElement, "time-since-message", this._timeIntervalString(patch.activeSince) + "\xa0ago");
            } else
                this.addTextToRow(rowElement, "latest-status-no-link", "Not started yet");

            if (patch.latestMessageTime)
                this.addTextToRow(rowElement, "time-since-message", this._timeIntervalString(patch.latestMessageTime) + "\xa0ago");

            this.addLinkToRow(rowElement, "bugzilla-link", "bugzilla", bugzilla.detailsURLForAttachment(patch.attachmentID));

            content.appendChild(rowElement);
        }

        this._addDividerToPopover(content);
        this._addBotsHeadingToPopover(queue, content);
        this._addDividerToPopover(content);

        var bots = queue.bots.slice(0).sort(function(a, b) { return (a.id < b.id) ? -1 : 1; });
        for (var i = 0, end = bots.length; i < end; ++i) {
            var bot = bots[i];

            var rowElement = document.createElement("div");

            this.addLinkToRow(rowElement, "bot-status-link", bot.id, bot.statusPageURL);
            this.addTextToRow(rowElement, "bot-status-description", this._timeIntervalString(bot.latestMessageTime) + "\xa0ago");

            content.appendChild(rowElement);
        }

        return content;
    },

    _presentPopoverForBubbleQueue: function(element, popover, queue)
    {
        if (queue.loadedDetailedStatus)
            var content = this._popoverContentForBubbleQueue(queue);
        else {
            var content = document.createElement("div");
            content.className = "bubble-server-popover";

            var loadingIndicator = document.createElement("div");
            loadingIndicator.className = "loading-indicator";
            loadingIndicator.textContent = "Loading\u2026";
            content.appendChild(loadingIndicator);

            queue.loadDetailedStatus(function() {
                popover.content = this._popoverContentForBubbleQueue(queue);
            }.bind(this));
        }

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);
        return true;
    },

    _updateQueues: function()
    {
        this.queues.forEach(function(queue) { queue.update(); });
    },

    _queueUpdated: function(event)
    {
        this.updateSoon();
    },
};
