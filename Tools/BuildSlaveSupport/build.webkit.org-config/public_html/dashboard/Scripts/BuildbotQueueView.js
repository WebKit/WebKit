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

BuildbotQueueView = function(debugQueues, releaseQueues)
{
    QueueView.call(this);

    this.releaseQueues = releaseQueues || [];
    this.debugQueues = debugQueues || [];

    this.releaseQueues.forEach(function(queue) {
        if (this.platform && this.platform != queue.platform)
            throw "A buildbot view may not contain queues for multiple platforms."
        else
            this.platform = queue.platform;
        queue.addEventListener(BuildbotQueue.Event.IterationsAdded, this._queueIterationsAdded, this);
    }.bind(this));

    this.debugQueues.forEach(function(queue) {
        if (this.platform && this.platform != queue.platform)
            throw "A buildbot view may not contain queues for multiple platforms."
        else
            this.platform = queue.platform;
        queue.addEventListener(BuildbotQueue.Event.IterationsAdded, this._queueIterationsAdded, this);
    }.bind(this));

    webkitTrac.addEventListener(Trac.Event.NewCommitsRecorded, this._newCommitsRecorded, this);
    if (typeof internalTrac != "undefined")
        internalTrac.addEventListener(Trac.Event.NewCommitsRecorded, this._newCommitsRecorded, this);
};

BaseObject.addConstructorFunctions(BuildbotQueueView);

BuildbotQueueView.UpdateInterval = 45000; // 45 seconds
BuildbotQueueView.UpdateSoonTimeout = 1000; // 1 second

BuildbotQueueView.prototype = {
    constructor: BuildbotQueueView,
    __proto__: QueueView.prototype,

    _latestFinishedIteration: function(queue)
    {
        for (var i = 0; i < queue.iterations.length; ++i) {
            var iteration = queue.iterations[i];
            if (!iteration.loaded || !iteration.finished)
                continue;
            return iteration;
        }
        return null;
    },

    _appendPendingRevisionCount: function(queue)
    {
        var latestFinishedIteration = this._latestFinishedIteration(queue);
        if (!latestFinishedIteration)
            return;

        var latestRecordedOpenSourceRevisionNumber = webkitTrac.latestRecordedRevisionNumber;
        if (!latestRecordedOpenSourceRevisionNumber)
            return;

        var openSourceRevisionsBehind = latestRecordedOpenSourceRevisionNumber - latestFinishedIteration.openSourceRevision;
        if (openSourceRevisionsBehind < 0)
            openSourceRevisionsBehind = 0;

        if (latestFinishedIteration.internalRevision) {
            var latestRecordedInternalRevisionNumber = internalTrac.latestRecordedRevisionNumber;
            if (!latestRecordedInternalRevisionNumber)
                return;

            var internalRevisionsBehind = latestRecordedInternalRevisionNumber - latestFinishedIteration.internalRevision;
            if (internalRevisionsBehind < 0)
                internalRevisionsBehind = 0;
            if (openSourceRevisionsBehind || internalRevisionsBehind) {
                var message = openSourceRevisionsBehind + " \uff0b " + internalRevisionsBehind + " revisions behind";
                var status = new StatusLineView(message, StatusLineView.Status.NoBubble);
                this.element.appendChild(status.element);
            }
        } else if (openSourceRevisionsBehind) {
            var message = openSourceRevisionsBehind + " " + (openSourceRevisionsBehind === 1 ? "revision behind" : "revisions behind");
            var status = new StatusLineView(message, StatusLineView.Status.NoBubble);
            this.element.appendChild(status.element);
        }

        if (status)
            new PopoverTracker(status.messageElement, this, queue);
    },

    presentPopoverForElement: function(element, popover, queue)
    {
        var latestFinishedIteration = this._latestFinishedIteration(queue);
        if (!latestFinishedIteration)
            return false;

        function lineForCommit(trac, commit)
        {
            var result = document.createElement("div");
            result.className = "pending-commit";

            var linkElement = document.createElement("a");
            linkElement.className = "revision";
            linkElement.href = trac.revisionURL(commit.revisionNumber);
            linkElement.target = "_blank";
            linkElement.textContent = "r" + commit.revisionNumber;
            result.appendChild(linkElement);

            var authorElement = document.createElement("span");
            authorElement.className = "author";
            authorElement.textContent = commit.author;
            result.appendChild(authorElement);

            var titleElement = document.createElement("span");
            titleElement.className = "title";
            titleElement.innerHTML = commit.title.innerHTML;
            result.appendChild(titleElement);

            return result;
        }

        var content = document.createElement("div");
        content.className = "pending-commits-popover";

        for (var i = webkitTrac.recordedCommits.length - 1; i >= 0; --i) {
            var commit = webkitTrac.recordedCommits[i];
            if (commit.revisionNumber <= latestFinishedIteration.openSourceRevision)
                break;

            content.appendChild(lineForCommit(webkitTrac, commit));
        }

        if (latestFinishedIteration.internalRevision && internalTrac.latestRecordedRevisionNumber) {
            if (latestFinishedIteration.internalRevision < internalTrac.latestRecordedRevisionNumber && content.hasChildNodes()) {
                var divider = document.createElement("div");
                divider.className = "divider";
                content.appendChild(divider);
            }

            for (var i = internalTrac.recordedCommits.length - 1; i >= 0; --i) {
                var commit = internalTrac.recordedCommits[i];
                if (commit.revisionNumber <= latestFinishedIteration.internalRevision)
                    break;

                content.appendChild(lineForCommit(internalTrac, commit));
            }
        }

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.present(rect, content, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);

        return true;
    },

    revisionLinksForIteration: function(iteration)
    {
        function linkForRevision(revision, trac)
        {
            var linkElement = document.createElement("a");
            linkElement.href = trac.revisionURL(revision);
            linkElement.target = "_blank";
            linkElement.textContent = "r" + revision;
            linkElement.classList.add("selectable");

            return linkElement;
        }

        console.assert(iteration.openSourceRevision);
        var openSourceLink = linkForRevision(iteration.openSourceRevision, webkitTrac);

        if (!iteration.internalRevision)
            return openSourceLink;

        var internalLink = linkForRevision(iteration.internalRevision, internalTrac);

        var fragment = document.createDocumentFragment();
        fragment.appendChild(openSourceLink);
        fragment.appendChild(document.createTextNode(" \uff0b "));
        fragment.appendChild(internalLink);
        return fragment;
    },

    _updateQueues: function()
    {
        this.releaseQueues.forEach(function(queue) { queue.update(); });
        this.debugQueues.forEach(function(queue) { queue.update(); });
    },

    _queueIterationsAdded: function(event)
    {
        this.updateSoon();

        event.data.addedIterations.forEach(function(iteration) {
            iteration.addEventListener(BuildbotIteration.Event.Updated, this._iterationUpdated, this);
        }.bind(this));
    },

    _iterationUpdated: function(event)
    {
        this.updateSoon();
    },
    
    _newCommitsRecorded: function(event)
    {
        this.updateSoon();
    }
};
