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
        queue.addEventListener(BuildbotQueue.Event.UnauthorizedAccess, this._unauthorizedAccess, this);
    }.bind(this));

    this.debugQueues.forEach(function(queue) {
        if (this.platform && this.platform != queue.platform)
            throw "A buildbot view may not contain queues for multiple platforms."
        else
            this.platform = queue.platform;
        queue.addEventListener(BuildbotQueue.Event.IterationsAdded, this._queueIterationsAdded, this);
        queue.addEventListener(BuildbotQueue.Event.UnauthorizedAccess, this._unauthorizedAccess, this);
    }.bind(this));

    webkitTrac.addEventListener(Trac.Event.CommitsUpdated, this._newCommitsRecorded, this);
    if (typeof internalTrac != "undefined")
        internalTrac.addEventListener(Trac.Event.CommitsUpdated, this._newCommitsRecorded, this);
};

BaseObject.addConstructorFunctions(BuildbotQueueView);

BuildbotQueueView.UpdateInterval = 45000; // 45 seconds
BuildbotQueueView.UpdateSoonTimeout = 1000; // 1 second

BuildbotQueueView.prototype = {
    constructor: BuildbotQueueView,
    __proto__: QueueView.prototype,

    _latestProductiveIteration: function(queue)
    {
        if (!queue.iterations.length)
            return null;
        if (queue.iterations[0].productive)
            return queue.iterations[0];
        return queue.iterations[0].previousProductiveIteration;
    },

    _appendUnauthorizedLineView: function(queue)
    {
        console.assert(queue.buildbot.needsAuthentication);
        console.assert(!queue.buildbot.isAuthenticated);
        var javascriptURL = "javascript:buildbots[" + buildbots.indexOf(queue.buildbot) + "].updateQueues(Buildbot.UpdateReason.Reauthenticate)";
        var status = new StatusLineView("unauthorized", StatusLineView.Status.Unauthorized, "", null, javascriptURL);
        this.element.appendChild(status.element);
    },

    _appendPendingRevisionCount: function(queue)
    {
        var latestProductiveIteration = this._latestProductiveIteration(queue);
        if (!latestProductiveIteration)
            return;

        var latestRecordedOpenSourceRevisionNumber = webkitTrac.latestRecordedRevisionNumber;
        if (!latestRecordedOpenSourceRevisionNumber || webkitTrac.oldestRecordedRevisionNumber > latestProductiveIteration.openSourceRevision) {
            webkitTrac.loadMoreHistoricalData();
            return;
        }

        // FIXME: To be 100% correct, we should also filter out changes that are ignored by
        // the queue, see _should_file_trigger_build in wkbuild.py.
        var totalRevisionsBehind = webkitTrac.commitsOnBranch(queue.branch.openSource, function(commit) { return commit.revisionNumber > latestProductiveIteration.openSourceRevision; }).length;

        if (latestProductiveIteration.internalRevision) {
            var latestRecordedInternalRevisionNumber = internalTrac.latestRecordedRevisionNumber;
            if (!latestRecordedInternalRevisionNumber || internalTrac.oldestRecordedRevisionNumber > latestProductiveIteration.internalRevision) {
                internalTrac.loadMoreHistoricalData();
                return;
            }

            totalRevisionsBehind += internalTrac.commitsOnBranch(queue.branch.internal, function(commit) { return commit.revisionNumber > latestProductiveIteration.internalRevision; }).length;
        }

        if (!totalRevisionsBehind)
            return;

        var messageElement = document.createElement("span"); // We can't just pass text to StatusLineView here, because we need an element that perfectly fits the text for popover positioning.
        messageElement.textContent = totalRevisionsBehind + " " + (totalRevisionsBehind === 1 ? "revision behind" : "revisions behind");
        var status = new StatusLineView(messageElement, StatusLineView.Status.NoBubble);
        this.element.appendChild(status.element);

        new PopoverTracker(messageElement, this._presentPopoverForPendingCommits.bind(this), queue);
    },

    _popoverLinesForCommitRange: function(trac, branch, firstRevisionNumber, lastRevisionNumber)
    {
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

        console.assert(trac.oldestRecordedRevisionNumber >= firstRevisionNumber);

        // FIXME: To be 100% correct, we should also filter out changes that are ignored by
        // the queue, see _should_file_trigger_build in wkbuild.py.
        var commits = trac.commitsOnBranch(branch, function(commit) { return commit.revisionNumber >= firstRevisionNumber && commit.revisionNumber <= lastRevisionNumber; });
        return commits.map(function(commit) {
            return lineForCommit(trac, commit);
        }, this).reverse();
    },

    _presentPopoverForPendingCommits: function(element, popover, queue)
    {
        var latestProductiveIteration = this._latestProductiveIteration(queue);
        if (!latestProductiveIteration)
            return false;

        var content = document.createElement("div");
        content.className = "commit-history-popover";

        var linesForOpenSource = this._popoverLinesForCommitRange(webkitTrac, queue.branch.openSource, latestProductiveIteration.openSourceRevision + 1, webkitTrac.latestRecordedRevisionNumber);
        for (var i = 0; i != linesForOpenSource.length; ++i)
            content.appendChild(linesForOpenSource[i]);

        var linesForInternal = [];
        if (latestProductiveIteration.internalRevision && internalTrac.latestRecordedRevisionNumber)
            var linesForInternal = this._popoverLinesForCommitRange(internalTrac, queue.branch.internal, latestProductiveIteration.internalRevision + 1, internalTrac.latestRecordedRevisionNumber);

        if (linesForOpenSource.length && linesForInternal.length)
            this._addDividerToPopover(content);

        for (var i = 0; i != linesForInternal.length; ++i)
            content.appendChild(linesForInternal[i]);

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);

        return true;
    },

    _presentPopoverForRevisionRange: function(element, popover, context)
    {
        var content = document.createElement("div");
        content.className = "commit-history-popover";

        // FIXME: Nothing guarantees that Trac has historical data for these revisions.
        var linesForCommits = this._popoverLinesForCommitRange(context.trac, context.branch, context.firstRevision, context.lastRevision);
        if (!linesForCommits.length)
            return false;

        var line = document.createElement("div");
        line.className = "title";
        line.textContent = "commits since previous result";
        content.appendChild(line);
        this._addDividerToPopover(content);

        for (var i = 0; i != linesForCommits.length; ++i)
            content.appendChild(linesForCommits[i]);

        var rect = Dashboard.Rect.rectFromClientRect(element.getBoundingClientRect());
        popover.content = content;
        popover.present(rect, [Dashboard.RectEdge.MIN_Y, Dashboard.RectEdge.MAX_Y, Dashboard.RectEdge.MAX_X, Dashboard.RectEdge.MIN_X]);

        return true;
    },

    _revisionContentWithPopoverForIteration: function(iteration, previousIteration, internal)
    {
        var content = document.createElement("span");
        content.textContent = "r" + (internal ? iteration.internalRevision : iteration.openSourceRevision);
        content.classList.add("revision-number");

        if (previousIteration) {
            var context = {
                trac: internal ? internalTrac : webkitTrac,
                branch: internal ? iteration.queue.branch.internal : iteration.queue.branch.openSource,
                firstRevision: (internal ? previousIteration.internalRevision : previousIteration.openSourceRevision) + 1,
                lastRevision: internal ? iteration.internalRevision : iteration.openSourceRevision
            };
            if (context.firstRevision <= context.lastRevision)
                new PopoverTracker(content, this._presentPopoverForRevisionRange.bind(this), context);
        }

        return content;
    },

    _addIterationHeadingToPopover: function(iteration, content, additionalText)
    {
        var title = document.createElement("div");
        title.className = "popover-iteration-heading";

        var queueName = document.createElement("span");
        queueName.className = "queue-name";
        queueName.textContent = iteration.queue.id;
        title.appendChild(queueName);

        var buildbotLink = document.createElement("a");
        buildbotLink.className = "buildbot-link";
        buildbotLink.textContent = "build #" + iteration.id;
        buildbotLink.href = iteration.queue.buildbot.buildPageURLForIteration(iteration);
        buildbotLink.target = "_blank";
        title.appendChild(buildbotLink);

        if (additionalText) {
            var additionalTextElement = document.createElement("span");
            additionalTextElement.className = "additional-text";
            additionalTextElement.textContent = additionalText;
            title.appendChild(additionalTextElement);
        }

        content.appendChild(title);
    },

    _addDividerToPopover: function(content)
    {
        var divider = document.createElement("div");
        divider.className = "divider";
        content.appendChild(divider);
    },

    revisionContentForIteration: function(iteration, previousDisplayedIteration)
    {
        console.assert(iteration.openSourceRevision);

        var openSourceContent = this._revisionContentWithPopoverForIteration(iteration, previousDisplayedIteration);

        if (!iteration.internalRevision)
            return openSourceContent;

        var internalContent = this._revisionContentWithPopoverForIteration(iteration, previousDisplayedIteration, true);

        var fragment = document.createDocumentFragment();
        fragment.appendChild(openSourceContent);
        fragment.appendChild(document.createTextNode(" \uff0b "));
        fragment.appendChild(internalContent);
        return fragment;
    },

    appendBuildStyle: function(queues, defaultLabel, appendFunction)
    {
        queues.forEach(function(queue) {
            var releaseLabel = document.createElement("a");
            releaseLabel.classList.add("queueLabel");
            releaseLabel.textContent = queue.heading ? queue.heading : defaultLabel;
            releaseLabel.href = queue.overviewURL;
            releaseLabel.target = "_blank";
            this.element.appendChild(releaseLabel);

            appendFunction.call(this, queue);
        }.bind(this));
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
            iteration.addEventListener(BuildbotIteration.Event.UnauthorizedAccess, this._unauthorizedAccess, this);
        }.bind(this));
    },

    _iterationUpdated: function(event)
    {
        this.updateSoon();
    },
    
    _newCommitsRecorded: function(event)
    {
        this.updateSoon();
    },

    _unauthorizedAccess: function(event)
    {
        this.updateSoon();
    }

};
